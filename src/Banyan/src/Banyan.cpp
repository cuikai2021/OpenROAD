#include "banyan/Banyan.h"

#include <chrono>
#include <sstream>
#include <spdlog/spdlog.h>
#include <boost/program_options.hpp>
#include <filesystem>
#include <string>

#include "banyan_db.pb.h"
#include "odb/db.h"
#include "odb/defin.h"
#include "odb/defout.h"
#include "odb/lefin.h"
#include "odb/lefout.h"
#include "odb/wOrder.h"
#include "ord/OpenRoad.hh"
#include "sta/StaMain.hh"
#include "lrl/Laurel.hh"
#include "utl/Logger.h"
#include "envflag/envflag.hpp"

IMPORT_BOOL_FLAG_WITH_DEFAULT(BANYAN_SUPPRESS_EXE_LOG);

namespace sta {
// Tcl files encoded into strings.
extern const char* Banyan_tcl_inits[];
}  // namespace sta

extern "C" {
extern int Banyan_Init(Tcl_Interp* interp);
}

namespace banyan {

std::string getFileName(std::string_view path)
{
  std::filesystem::path p(path);
  return p.filename().string();
}

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
const char BANYAN_EXE_DEFAULT[] = "/build/bin/banyan";
const char BANYAN_PBDEF_DEFAULT[] = "/build/bin/bypbdef";

std::string getExe(const char* exeName)
{
  if (!std::filesystem::exists(exeName)) {
    spdlog::error("Banyan executable not found: {}", exeName);
    exit(1);
  }
  return std::string(exeName);
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

int exec(utl::Logger* logger, const std::vector<std::string>& args, const char* exeName)
{
  std::string exe = getExe(exeName);
  int ec = 0;
  if (!BANYAN_SUPPRESS_EXE_LOG()) {
    ec = boost::process::system(exe, boost::process::args(args));
  } else {
    ec = boost::process::system(exe, boost::process::args(args),
        boost::process::std_out > boost::process::null,
        boost::process::std_err > boost::process::null);
  }
  return ec;
}

int Banyan::main(std::vector<std::string> args, bool updateDB)
{
  std::string outPbFilePath = "";
  std::vector<std::string> unregister;
  
  namespace po = boost::program_options;
  po::options_description desc("Banyan");
  desc.add_options()                                                         //
      ("help,h", "")                                                         //
      ("timing_driven", "")                                                  //
      ("save_db", po::value<std::string>(&outPbFilePath), "")                //
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
    exec(logger_, args, BANYAN_EXE_DEFAULT);
    return HELP;
  } else if (command == "") {
    logger_->error(utl::BY, 1, "subcommand is required but missing");
  }

  if(vm.count("db") || vm.count("techlef") || vm.count("lef")|| vm.count("def")) {
    logger_->error(utl::BY, 2, "The --db, --techlef, --lef, --def options is not supported. Please use read_lef, read_def to read the design.");
  }

  args.push_back("--techlef");
  args.push_back(tech_lef_file_name_);
  for (const auto& lef : other_lef_file_names_) {
    args.push_back("--lef");
    args.push_back(lef);
  }
  for (const auto& lef : other_tech_lef_file_names_) {
    args.push_back("--lef");
    args.push_back(lef);
  }
  auto defFilepath = genTempFilePath(logger_, ".def");
  ord::OpenRoad::openRoad()->writeDef(defFilepath.c_str(), "5.8");
  args.push_back("--def");
  args.push_back(defFilepath);

  if (updateDB && !vm.count("save_db")) {
    if (outPbFilePath == "") {
      outPbFilePath = genTempFilePath(logger_, ".pb");
    }
    args.push_back("--save_db");
    args.push_back(outPbFilePath);
  }
  defer _(nullptr, [&](...) {
    if (updateDB) {
      logger_->info(utl::BY, 1, "Writing banyan result {} to eden ...", outPbFilePath);
      loaddb(outPbFilePath);
    }
  });

  return exec(logger_, args,BANYAN_EXE_DEFAULT);
}

int Banyan::cmap(std::string_view cmapPb, bool cgr_mode, std::string_view max_layer)
{
  std::vector<std::string> args;
  args.push_back("report_routability");

  if (!max_layer.empty()) {
    args.push_back("--max_layer");
    args.emplace_back(max_layer);
  }

  args.push_back("--log_level");
  args.push_back("info");
  args.push_back("--gmap_out_protobuf");
  args.emplace_back(cmapPb);
  if (!cgr_mode) {
    args.push_back("--disable_mcf_upt");
    args.push_back("--baby_pa");
  }

  return main(args, false);
}

void Banyan::clearWires(bool excludeClock, bool excludeSignal)
{
  auto block = db_->getChip()->getBlock();
  for (auto net : block->getNets()) {
    if (net->isSpecial()) {
      continue;
    }
    if (excludeClock && net->getSigType() == odb::dbSigType::CLOCK) {
      continue;
    }
    if (excludeSignal && net->getSigType() == odb::dbSigType::SIGNAL) {
      continue;
    }
    if (auto wire = net->getWire(); wire != nullptr) {
      odb::dbWire::destroy(wire);
    }
  }
}

odb::dbDatabase* Banyan::createODB(std::string_view deffile)
{
  auto newdb = odb::dbDatabase::create();
  odb::lefin lef_reader(newdb, logger_, false);
  std::vector<odb::dbLib*> search_libs;
  auto techlib
      = lef_reader.createTechAndLib(getFileName(tech_lef_file_name_).c_str(),
                                    getFileName(tech_lef_file_name_).c_str(),
                                    tech_lef_file_name_.c_str());
  if (techlib != nullptr) {
    search_libs.push_back(techlib);
  }
  auto tech = newdb->getTech();
  for (auto libfile : other_lef_file_names_) {
    if (auto lib = lef_reader.createLib(
            tech, getFileName(libfile).c_str(), libfile.c_str());
        lib != nullptr) {
      search_libs.push_back(lib);
    }
  }
  for (auto techfile : other_tech_lef_file_names_) {
    lef_reader.updateTech(tech, techfile.c_str());
  }
  odb::defin def_reader(newdb, logger_);
  if (auto chip
      = def_reader.createChip(search_libs, std::string(deffile).c_str(), tech);
      chip == nullptr) {
    odb::dbDatabase::destroy(newdb);
    logger_->error(utl::BY, 2, "create chip fail, def file {}", deffile);
  }
  return newdb;
}

banyan_def::DesignExchangeFormat Banyan::def2pbdef(std::string deffile,
                                                   bool orderWires)
{
  std::vector<std::string> args;
  auto pbDefFilePath = genTempFilePath(logger_, ".pbdef");
  args.push_back("--from");
  args.push_back(deffile);
  args.push_back("--to");
  args.push_back(pbDefFilePath);
  if (auto status = exec(logger_, args, BANYAN_PBDEF_DEFAULT); status != 0) {
    logger_->error(utl::BY, 3, "exec {} fail with exit code {}", BANYAN_PBDEF_DEFAULT, status);
  }
  banyan_def::DesignExchangeFormat pbdef;
  std::ifstream outfs;
  outfs.open(pbDefFilePath);
  pbdef.ParseFromIstream(&outfs);
  outfs.close();
  //TODO sort pbdef.wires
  return pbdef;
}

std::string Banyan::pbdef2def(const banyan_def::DesignExchangeFormat& pbdef)
{
  std::vector<std::string> args;
  auto pbDefFilePath = genTempFilePath(logger_, ".pbdef");
  std::ofstream output(pbDefFilePath, std::ios::out | std::ios::binary);
  if (!pbdef.SerializeToOstream(&output)) {
    logger_->error(utl::BY, 4, "create pbdef {} fail", pbDefFilePath);
  }
  output.close();
  auto outDefFilePath = genTempFilePath(logger_, ".def");
  args.push_back("--from");
  args.push_back(pbDefFilePath);
  args.push_back("--to");
  args.push_back(outDefFilePath);
  if (auto status = exec(logger_, args, BANYAN_PBDEF_DEFAULT); status != 0) {
    logger_->error(utl::BY, 5, "exec {} fail with exit code {}", BANYAN_PBDEF_DEFAULT, status);
  }
  return outDefFilePath;
}

void Banyan::loaddb(std::string dbflie)
{
  BanyanCase bycase;
  std::ifstream outfs;
  outfs.open(dbflie);
  bycase.ParseFromIstream(&outfs);
  outfs.close();
  loaddef(pbdef2def(bycase.design()));
}

void Banyan::loaddef(std::string deffile)
{
  auto newODB = createODB(deffile);
  for (auto net : newODB->getChip()->getBlock()->getNets()) {
    auto oriNet = db_->getChip()->getBlock()->findNet(net->getConstName());
    assert(oriNet != nullptr);
    laurel_->cacheNet(oriNet);
    if (auto oriWire = oriNet->getWire(); oriWire != nullptr) {
      odb::dbWire::destroy(oriWire);
    }
    if (auto wire = net->getWire(); wire != nullptr) {
      std::vector<int> rawdata;
      std::vector<unsigned char> rawOpCodes;
      wire->getRawWireData(rawdata, rawOpCodes);
      auto oriWire = odb::dbWire::create(oriNet);
      oriWire->setRawWireData(rawdata, rawOpCodes);
    }
    laurel_->diffNet(oriNet);
  }
  odb::dbDatabase::destroy(newODB);
}

}  // namespace banyan
