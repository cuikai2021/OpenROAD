#include "banyan/Banyan.h"

#include <boost/program_options.hpp>
#include <chrono>
#include <iostream>
#include <sstream>

#include "DBAdapter.hpp"
#include "Global.hpp"
#include "GridlessMaze.hpp"
#include "MergeWireSegment.hpp"
#include "DrcPatch.hpp"
#include "PinCluster.hpp"
#include "banyan_instance_movement.pb.h"
#include "magic_enum.hpp"
#include "odb/db.h"
#include "odb/lefout.h"
#include "ord/OpenRoad.hh"
#include "sta/StaMain.hh"
#include "cgr/congestionRepair.hpp"
#include "utl/Logger.h"

namespace sta {
// Tcl files encoded into strings.
extern const char* Banyan_tcl_inits[];
}  // namespace sta

extern "C" {
extern int Banyan_Init(Tcl_Interp* interp);
}

namespace banyan {

constexpr int SUCCESS = 0;
constexpr int ERROR = -1;
constexpr int HELP = -2;

using defer = std::shared_ptr<void>;

Banyan::Banyan()
{
}

Banyan::~Banyan()
{
}

void Banyan::init(Tcl_Interp* tcl_interp,
                  odb::dbDatabase* db,
                  stt::SteinerTreeBuilder* stt_builder,
                  utl::Logger* logger,
                  lrl::Laurel* laurel)
{
  db_ = db;
  stt_builder_ = stt_builder;
  logger_ = logger;
  laurel_ = laurel;
  // Define swig TCL commands.
  Banyan_Init(tcl_interp);
  sta::evalTclInit(tcl_interp, sta::Banyan_tcl_inits);
}

std::string getExe()
{
  const char BANYAN_EXE[] = "BANYAN_EXE";
  const char BANYAN_EXE_DEFAULT[] = "/build/bin/banyan";
  const char* exe = std::getenv(BANYAN_EXE);
  if (exe == nullptr) {
    exe = BANYAN_EXE_DEFAULT;
  }
  return std::string(exe);
}

std::string genTempFilePath(utl::Logger* logger, std::string fileExt)
{
  if (!std::filesystem::exists(BANYAN_TMP_FOLDER)) {
    if (!std::filesystem::create_directories(BANYAN_TMP_FOLDER)) {
      logger->error(utl::BY, 1, "create {} dir fail", BANYAN_TMP_FOLDER);
    }
  }

  const auto now = std::chrono::system_clock::now();

  std::stringstream ss;

  ss << BANYAN_TMP_FOLDER
     << std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch())
            .count()
     << fileExt;

  return ss.str();
}

int exec(utl::Logger* logger, const std::vector<std::string>& args)
{
  std::string exe = std::move(getExe());
  int ec = boost::process::system(exe, boost::process::args(args));
  switch (ec) {
    case 0:
      logger->info(utl::BY, 1, "Banyan done");
      // intend to do nothing
      return ec;
    default:
      break;
  }
  logger->error(utl::BY, 1, "Run banyan error with exit code: {}", ec);
  return ec;
}

int Banyan::main(std::vector<std::string> args, bool updateDB)
{
  std::string techlef = "";
  std::string outPbFilePath = "";
  std::vector<std::string> unregister;

  namespace po = boost::program_options;
  po::options_description desc("Banyan");
  desc.add_options()                                                         //
      ("help,h", "")                                                         //
      ("techlef", po::value<std::string>(&techlef), "")                      //
      ("output", po::value<std::string>(&outPbFilePath), "")                 //
      ("unregister", po::value<std::vector<std::string>>(&unregister), "");  //

  po::positional_options_description posDesc;
  posDesc.add("unregister", -1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(args)
                  .options(desc)
                  .positional(posDesc)
                  .allow_unregistered()
                  .run(),
              vm);

    po::notify(vm);
  } catch (boost::program_options::required_option& e) {
    return ERROR;
  } catch (std::exception& e) {
    logger_->error(utl::BY, 1, "ERROR: {}", e.what());
    return ERROR;
  }

  std::string command = "";
  if (unregister.size() > 0) {
    command = unregister[0];
  }
  if (command == "help" || vm.count("help")) {
    exec(logger_, args);
    return HELP;
  } else if (command == "") {
    logger_->error(utl::BY, 1, "subcommand is required but missing");
  } else if (command != "route") {
    logger_->error(utl::BY, 1, "unknow banyan subcommand '{}'", command);
  }

  if (!vm.count("techlef")) {
    args.push_back("--techlef");
    args.push_back(techfile_name_);
  }

  OpenDBAdapter dbAdapter(db_, stt_builder_, logger_, laurel_, false);
  auto design = dbAdapter.getDesign();
  BanyanCase byCase;
  byCase.set_allocated_design(design.get());
  design.release();
  std::string inPbFilePath = genTempFilePath(logger_, ".pb");
  std::ofstream pbfs;
  pbfs.open(inPbFilePath,
            std::ofstream::in | std::ofstream::out | std::ofstream::trunc);
  if (!byCase.SerializeToOstream(&pbfs)) {
    logger_->error(utl::BY, 1, "save protobuf file to {} fail", inPbFilePath);
  }
  pbfs.close();
  args.push_back("--protobuf");
  args.push_back(inPbFilePath);

  if (updateDB) {
    if (outPbFilePath == "") {
      outPbFilePath = genTempFilePath(logger_, ".pb");
    }
    args.push_back("--output");
    args.push_back(outPbFilePath);
  }

  defer _(nullptr, [&](...) {
    if (updateDB) {
      logger_->info(utl::BY, 1, "Writing banyan result to eden ...");
      BanyanCase byrst;
      std::ifstream outfs;
      outfs.open(outPbFilePath);
      byrst.ParseFromIstream(&outfs);
      outfs.close();
      dbAdapter.updateDB(byrst.design());
    }
  });

  return exec(logger_, args);
}

int Banyan::cmap(const std::string& cmapPb, bool cgr_mode, const std::string& instMovementPb)
{
  std::vector<std::string> args;
  args.push_back("route");
  args.push_back("--techlef");
  args.push_back(techfile_name_);

  const auto max_layer = ord::OpenRoad::openRoad()->getCongestionRepair()->getRoutingMaxLayer();
  if (!max_layer.empty()) {
    args.push_back("--max_layer");
    args.push_back(max_layer);
  }

  args.push_back("--log_level");
  args.push_back("info");
  args.push_back("--report_cmap");
  args.push_back(cmapPb);
  args.push_back("--baby_banyan");
  args.push_back("true");
  args.push_back("--disable_maze_top");
  args.push_back("true");
  if (!cgr_mode) {
    args.push_back("--disable_mcf_upt");
    args.push_back("true");
    args.push_back("--baby_pa");
    args.push_back("true");
  }
  args.push_back("--cmap_use_corearea");
  args.push_back("true");

  if (instMovementPb != "") {
    args.push_back("--report_inst_movement");
    args.push_back(instMovementPb);
  }

  return main(args, false);
}

int Banyan::m1GridlessMaze()
{
  auto gm = GridlessMaze(db_, logger_, GridlessMaze::SearchLimitLevel::MEDIUM);
  auto res = gm.Run();
  GridlessMaze::updateDB(db_, res);
  return SUCCESS;
}

void Banyan::mergeWireSegment()
{
  auto mws = MergeWireSegment(db_, logger_);
  mws.Run();
}

int Banyan::instanceMovement(std::vector<std::string> args)
{
  std::string instanceMovementPb = "";

  namespace po = boost::program_options;
  po::options_description desc("Banyan");
  desc.add_options()  //
      ("report_inst_movement",
       po::value<std::string>(&instanceMovementPb)->required(),
       "");  //

  po::variables_map vm;
  try {
    po::store(
        po::command_line_parser(args).options(desc).allow_unregistered().run(),
        vm);

    po::notify(vm);
  } catch (std::exception& e) {
    logger_->error(utl::BY, 1, "ERROR: {}", e.what());
    return ERROR;
  }

  main(args, false);

  InstanceMovement im;
  std::ifstream outfs;
  outfs.open(instanceMovementPb);
  if (!im.ParseFromIstream(&outfs)) {
    logger_->error(utl::BY, 1, "parsing inst movement protobuf file error");
  }
  outfs.close();

  auto chip = db_->getChip();
  auto block = chip->getBlock();

  for (auto instance : im.instances()) {
    auto dbInst = block->findInst(instance.name().c_str());

    int x = 0;
    int y = 0;
    dbInst->getLocation(x, y);

    auto dbBBox = dbInst->getBBox();
    int movedx = instance.x() - dbBBox->getDX() / 2;
    int movedy = instance.y() - dbBBox->getDY() / 2;

    logger_->info(utl::BY,
                  1,
                  "instance {} from ({},{}) to ({},{})",
                  dbInst->getName(),
                  x,
                  y,
                  movedx,
                  movedy);

    dbInst->setLocation(movedx, movedy);
  }

  return SUCCESS;
}

void Banyan::drcPatch()
{
  auto dp = DrcPatch(db_, logger_);
  dp.run();
}

}  // namespace banyan
