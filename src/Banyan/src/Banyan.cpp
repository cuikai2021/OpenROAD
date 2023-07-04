#include "banyan/Banyan.h"

#include <boost/program_options.hpp>
#include <chrono>
#include <iostream>
#include <sstream>

#include <spdlog/spdlog.h>

#include "BanyanGraphics.hpp"
#include "ByAdapter.hpp"
#include "DBAdapter.hpp"
#include "DrcPatch.hpp"
#include "Global.hpp"
#include "GridlessMaze.hpp"
#include "PinCluster.hpp"
#include "UnionFind.hpp"
#include "ViaCandidate.hpp"
#include "banyan_instance_movement.pb.h"
#include "cgr/congestionRepair.hpp"
#include "magic_enum.hpp"
#include "odb/db.h"
#include "odb/lefout.h"
#include "ord/OpenRoad.hh"
#include "sta/StaMain.hh"
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
  if (!std::filesystem::exists(exe)) {
    spdlog::error("Banyan executable not found: {}", exe);
    exit(1);
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
  std::string inPbFilePath = "";
  std::string outPbFilePath = "";
  std::vector<std::string> unregister;

  namespace po = boost::program_options;
  po::options_description desc("Banyan");
  desc.add_options()                                                         //
      ("help,h", "")                                                         //
      ("techlef", po::value<std::string>(&techlef), "")                      //
      ("save_db", po::value<std::string>(&outPbFilePath), "")                //
      ("db", po::value<std::string>(&inPbFilePath), "")                      //
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
  }

  if (!vm.count("techlef")) {
    args.push_back("--techlef");
    args.push_back(techfile_name_);
  }

  OpenDBAdapter dbAdapter(db_, stt_builder_, logger_, laurel_, false);

  if (!vm.count("db")) {
    dbAdapter.loadDesignFromODB();
    auto design = dbAdapter.getDesign();
    BanyanCase byCase;
    byCase.set_allocated_design(design.get());
    design.release();
    inPbFilePath = genTempFilePath(logger_, ".pb");
    std::ofstream pbfs;
    pbfs.open(inPbFilePath,
              std::ofstream::in | std::ofstream::out | std::ofstream::trunc);
    if (!byCase.SerializeToOstream(&pbfs)) {
      logger_->error(utl::BY, 1, "save protobuf file to {} fail", inPbFilePath);
    }
    pbfs.close();
    args.push_back("--db");
    args.push_back(inPbFilePath);
  } else {
    dbAdapter.loadDesignFromBanyanDB(inPbFilePath);
  }

  if (updateDB && !vm.count("save_db")) {
    if (outPbFilePath == "") {
      outPbFilePath = genTempFilePath(logger_, ".pb");
    }
    args.push_back("--save_db");
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

int Banyan::cmap(const std::string& cmapPb, bool cgr_mode)
{
  std::vector<std::string> args;
  args.push_back("route");
  args.push_back("--techlef");
  args.push_back(techfile_name_);

  const auto max_layer
      = ord::OpenRoad::openRoad()->getCongestionRepair()->getRoutingMaxLayer();
  if (!max_layer.empty()) {
    args.push_back("--max_layer");
    args.push_back(max_layer);
  }

  args.push_back("--log_level");
  args.push_back("info");
  args.push_back("--report_cmap");
  args.push_back(cmapPb);
  args.push_back("--baby_banyan");
  if (!cgr_mode) {
    args.push_back("--disable_mcf_upt");
    args.push_back("--baby_pa");
  }

  return main(args, false);
}

int Banyan::m1GridlessMaze()
{
  auto bydb = ByAdapter(db_, logger_).getByDb();
  auto gm = GridlessMaze(db_, bydb, logger_, GridlessMaze::MEDIUN);
  gm.run();
  ByAdapter::updateWire(db_, bydb);
  return SUCCESS;
}

void Banyan::mergeWireSegment()
{
  auto bydb = ByAdapter(db_, logger_).getByDb();
  for (auto& net : bydb->getNets()) {
    if (net->isSpecial()) {
      continue;
    }
    std::vector<std::shared_ptr<ByBox>> rectWires;
    std::unordered_map<std::string, std::vector<std::shared_ptr<ByBox>>>
        layerWire;
    for (auto& wire : net->getWires()) {
      auto lineWidth
          = db_->getTech()->findLayer(wire->getLayerName().c_str())->getWidth();
      if (wire->isLineBox(lineWidth)) {
        layerWire[wire->getLayerName()].emplace_back(wire);
      } else {
        rectWires.emplace_back(wire);
      }
    }
    for (auto& [layer, wires] : layerWire) {
      BsPolygon90Set s;
      for (const auto& b : wires) {
        s += *b;
      }
      auto lineWidth = db_->getTech()->findLayer(layer.c_str())->getWidth();
      std::vector<ByBox> res;
      bsp::get_max_rectangles(res, s);
      auto info = std::make_shared<ByBoxInfo>(layer,
                                              net->getName(),
                                              std::nullopt,
                                              std::nullopt,
                                              std::nullopt,
                                              ByBoxInfo::WIRE);
      for (const auto& b : res) {
        if (b.isLineBox(lineWidth)) {
          rectWires.emplace_back(std::make_shared<ByBox>(
              b.xMin(), b.yMin(), b.xMax(), b.yMax(), info));
        }
      }
    }
    net->mutableWires() = rectWires;
  }
  logger_->info(utl::BY, 1, "wire merged!");
  ByAdapter::updateWire(db_, bydb);
}

void Banyan::runGui()
{
  auto bydb = ByAdapter(db_, logger_).getByDb();
  std::unique_ptr<BanyanGraphics> graphics;
  if (BanyanGraphics::guiActive()) {
    graphics = std::make_unique<BanyanGraphics>(logger_);
  }
  if (graphics) {
    graphics->status("Open nets polygons");
    auto netPolygons = bydb->getOpenNetPolygons();
    logger_->info(utl::BY, 1, "open:{}", netPolygons.size());
    std::unordered_map<std::string, std::vector<std::pair<BsPoint, BsPoint>>>
        netMst;
    for (const auto& [netName, netPls] : netPolygons) {
      netMst[netName] = ByDb::getPolygonsMst(netPls);
    }
    graphics->drawPolygons(netPolygons, netMst);
  }
  return;
}


void Banyan::drcPatch()
{
  auto dp = DrcPatch(db_, logger_);
  dp.run();
}

void Banyan::clearWires()
{
  auto block = db_->getChip()->getBlock();
  for (auto net : block->getNets()) {
    auto wire = net->getWire();
    if (wire != nullptr) {
      odb::dbWire::destroy(wire);
    }
  }
}

}  // namespace banyan
