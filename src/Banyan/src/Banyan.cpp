#include "banyan/Banyan.h"

#include <chrono>
#include <functional>
#include <sstream>
#include <spdlog/spdlog.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <utility>

#include "BanyanTimer.hpp"
#include "banyan_db.pb.h"
#include "odb/db.h"
#include "odb/defin.h"
#include "odb/defout.h"
#include "odb/lefin.h"
#include "odb/lefout.h"
#include "ord/OpenRoad.hh"
#include "sta/StaMain.hh"
#include "utl/Logger.h"
#include "envflag/envflag.hpp"
#include "WireSorter.hpp"
#include "DBAdapter.hpp"

IMPORT_BOOL_FLAG_WITH_DEFAULT(BANYAN_SUPPRESS_EXE_LOG);

namespace sta {
// Tcl files encoded into strings.
extern const char* Banyan_tcl_inits[];
}  // namespace sta

extern "C" {
extern int Banyan_Init(Tcl_Interp* interp);
}

namespace banyan {

bool designEqual(Design& oldDesign, Design& newDesign, std::string& differences, bool exceptWires)
{

  google::protobuf::util::MessageDifferencer differencer;

  differencer.set_report_moves(false);
  differencer.set_report_ignores(false);

  auto designDesc = Design::descriptor();
  differencer.TreatAsSet(designDesc->FindFieldByName("tracks"));
  differencer.TreatAsSet(designDesc->FindFieldByName("nets"));
  differencer.TreatAsSet(designDesc->FindFieldByName("specialNets"));
  differencer.TreatAsSet(designDesc->FindFieldByName("blockages"));
  differencer.TreatAsSet(designDesc->FindFieldByName("vias"));
  differencer.TreatAsSet(designDesc->FindFieldByName("instances"));
  differencer.TreatAsSet(designDesc->FindFieldByName("pins"));
  differencer.TreatAsSet(designDesc->FindFieldByName("rows"));
  differencer.TreatAsSet(designDesc->FindFieldByName("viaRules"));
  differencer.TreatAsSet(designDesc->FindFieldByName("nonDefaultRules"));

  auto specialNetDesc = SpecialNet::descriptor();
  const google::protobuf::FieldDescriptor* spNetWiresField = specialNetDesc->FindFieldByName("wires");
  differencer.TreatAsSet(spNetWiresField);
  const google::protobuf::FieldDescriptor* spNetPinsField = specialNetDesc->FindFieldByName("pins");
  differencer.TreatAsSet(spNetPinsField);

  auto netDesc = Net::descriptor();
  const google::protobuf::FieldDescriptor* netWiresField = netDesc->FindFieldByName("wires");
  if (exceptWires) differencer.IgnoreField(netWiresField);
  else {
    WireSorter::designWireOrdering(oldDesign);
    WireSorter::designWireOrdering(newDesign);
  }
  const google::protobuf::FieldDescriptor* netPinsField = netDesc->FindFieldByName("pins");
  differencer.TreatAsSet(netPinsField);


  std::string diff_report;
  differencer.ReportDifferencesToString(&diff_report);
  auto equal = differencer.Compare(oldDesign, newDesign);
  if (!equal) {
    differences = diff_report;
  }
  return equal;
}

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
  std::string exe = getExe();
  int ec = 0;
  if (!BANYAN_SUPPRESS_EXE_LOG()) {
    ec = boost::process::system(exe, boost::process::args(args));
  } else {
    ec = boost::process::system(exe, boost::process::args(args),
        boost::process::std_out > boost::process::null,
        boost::process::std_err > boost::process::null);
  }

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
  std::string inPbFilePath = "";
  std::string outPbFilePath = "";
  std::vector<std::string> unregister;

  namespace po = boost::program_options;
  po::options_description desc("Banyan");
  desc.add_options()                                                         //
      ("help,h", "")                                                         //
      ("timing_driven", "")                                                  //
      ("techlef", po::value<std::string>(), "")                              //
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
    args.push_back(tech_lef_file_name_);
  }

  // 重复路径可在 banyan bin 处处理。应该不存在 LEF 冲突的情况。
  for (const auto& lef : other_lef_file_names_) {
    args.push_back("--lef");
    args.push_back(lef);
  }

  // bool timingDriven = vm.count("timing_driven");
  OpenDBAdapter dbAdapter(db_, logger_, laurel_);

  if (!vm.count("db")) {
    dbAdapter.loadDesignFromODB();
    auto design = dbAdapter.getDesign();
    BanyanTimer byTimer(db_, logger_, laurel_, design);
    auto timeReport = byTimer.getTimeReport();

    BanyanCase byCase;
    byCase.set_allocated_design(design.release());
    byCase.set_allocated_timingreport(timeReport.release());
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
      logger_->info(
          utl::BY, 1, "Writing banyan result {} to eden ...", outPbFilePath);
      BanyanCase byrst;
      std::ifstream outfs;
      outfs.open(outPbFilePath);
      byrst.ParseFromIstream(&outfs);
      outfs.close();
      dbAdapter.updateDB(*byrst.mutable_design());
    }
  });

  return exec(logger_, args);
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
    auto wire = net->getWire();
    if (wire != nullptr) {
      odb::dbWire::destroy(wire);
    }
  }
}

void Banyan::savedb(std::string dbfile)
{
  OpenDBAdapter dbAdapter(db_, logger_, laurel_);
  dbAdapter.loadDesignFromODB();
  auto design = dbAdapter.getDesign();
  BanyanTimer byTimer(db_, logger_, laurel_, design);
  auto timeReport = byTimer.getTimeReport();
  
  BanyanCase byCase;
  byCase.set_allocated_design(design.release());
  byCase.set_allocated_timingreport(timeReport.release());

  std::ofstream pbfs;
  pbfs.open(dbfile,
            std::ofstream::in | std::ofstream::out | std::ofstream::trunc);
  if (!byCase.SerializeToOstream(&pbfs)) {
    logger_->error(utl::BY, 1, "save protobuf file to {} fail", dbfile);
  }
  pbfs.close();
}

void Banyan::loaddb(std::string dbflie)
{
  OpenDBAdapter dbAdapter(db_, logger_, laurel_);
  dbAdapter.loadDesignFromBanyanDB(dbflie);
  auto design = dbAdapter.getDesign();
  dbAdapter.updateDB(*design);
}

odb::dbDatabase* Banyan::createODB(std::string_view deffile)
{
  auto newdb = odb::dbDatabase::create();
  odb::lefin lef_reader(newdb, logger_, false);
  std::vector<odb::dbLib*> search_libs;
  auto techlib = lef_reader.createTechAndLib(getFileName(tech_lef_file_name_).c_str(), getFileName(tech_lef_file_name_).c_str(), tech_lef_file_name_.c_str());
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
  odb::defin def_reader(newdb, logger_);
  if (auto chip
      = def_reader.createChip(search_libs, std::string(deffile).c_str(), tech);
      chip == nullptr) {
    odb::dbDatabase::destroy(newdb);
    logger_->error(utl::BY, 2, "create chip fail, def file {}", deffile);
  }
  return newdb;
}
std::unique_ptr<Design> Banyan::createDesign(std::string_view deffile){
  odb::dbDatabase* newDb = createODB(deffile);
  OpenDBAdapter dbAdapter(newDb, logger_, nullptr); //laurel设置为空确保 dbAdapter 不会调用updateDB
  dbAdapter.loadDesignFromODB();
  odb::dbDatabase::destroy(newDb);
  return dbAdapter.getDesign();
}

void Banyan::loaddef(std::string_view deffile)
{
  auto newDesign = createDesign(deffile);
  OpenDBAdapter dbAdapter(db_, logger_, laurel_);
  dbAdapter.updateDB(*newDesign);
}
void Banyan::diffdefs(std::string_view deffileA,std::string_view deffileB, bool exceptWires)
{
  std::string messagesDiff = "";
  auto designA = createDesign(deffileA);
  auto designB = createDesign(deffileB);

  bool result = designEqual(*designA,*designB,messagesDiff, exceptWires);
  if (result)this->logger_->info(utl::BY,3,"No differences found.");
  else this->logger_->info(utl::BY,
                           3,
                           "The two designs have inconsistent fields. {}",
                           messagesDiff);
}

}  // namespace banyan
