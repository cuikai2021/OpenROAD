#include "banyan/Banyan.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <sstream>
#include <spdlog/spdlog.h>
#include <boost/program_options.hpp>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "banyan_db.pb.h"
#include "BanyanIncrRoute.hpp"
#include "odb/dbTypes.h"
#include "fassert.hpp"
#include "odb/geom.h"
#include "../../../../linden/include/linden/eco_placer.hpp"
#include "lrl/CircuitModifier/ModifierTimeline.hh"
#include "lrl/CircuitModifier/STANetwork.hh"
#include "lrl/Sta.hh"
#include "odb/db.h"
#include "odb/defin.h"
#include "odb/defout.h"
#include "odb/lefin.h"
#include "odb/lefout.h"
#include "odb/wOrder.h"
#include "ord/OpenRoad.hh"
#include "ord/Timing.h"
#include "rsz/Resizer.hh"
#include "sta/Liberty.hh"
#include "sta/StaMain.hh"
#include "lrl/Laurel.hh"
#include "utl/Logger.h"
#include "envflag/envflag.hpp"
#include "congestion_optimizer.hpp"
#include "grt/GlobalRouter.h"

IMPORT_BOOL_FLAG_WITH_DEFAULT(BANYAN_SUPPRESS_EXE_LOG);
IMPORT_STR_FLAG_WITH_DEFAULT(DEBLAIS_ROOT_DIR);

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
const char BANYAN_AUTO_APB[] = "/build/bin/auto_apb";
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

std::string genTempDir(utl::Logger* logger)
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
    <<"/";
  auto dir = ss.str();
  if (!std::filesystem::exists(dir)) {
    if (!std::filesystem::create_directories(dir)) {
      logger->error(utl::BY, 1, "create {} dir fail", dir);
    }
  }
  return dir;
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
  logger->info(utl::BY, 1010, "Banyan exec {} done, status code {}", exeName, ec);
  if (ec != 0) {
    std::exit(ec);
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

int Banyan::cmap(std::string_view cmapPb, std::string_view max_layer)
{
  std::vector<std::string> args;
  args.push_back("report_routability");

  if (!max_layer.empty()) {
    args.push_back("--max_layer");
    args.emplace_back(max_layer);
  }
  args.push_back("--disable_mcf_upt");
  args.push_back("--baby_pa");

  args.push_back("--log_level");
  args.push_back("info");
  args.push_back("--gmap_out_protobuf");
  args.emplace_back(cmapPb);

  return main(args, false);
}

int Banyan::repair_congestion(std::string_view cmapPb,
                              std::string_view max_layer,
                              bool nopa,
                              std::optional<size_t> mm_iter,
                              std::optional<size_t> gcell_size)
{
  std::vector<std::string> args;
  args.emplace_back("repair_congestion");
  args.emplace_back("--enable_dynamic_local");
  if (nopa) {
    args.emplace_back("--no_pa");
  }
  if (!max_layer.empty()) {
    args.emplace_back("--max_layer");
    args.emplace_back(max_layer);
  }

  args.emplace_back("--log_level");
  args.emplace_back("info");

  args.emplace_back("--out_protobuf");
  args.emplace_back(cmapPb);

  if (mm_iter.has_value()) {
    args.emplace_back("--mm_iter");
    args.emplace_back(fmt::format("{}", *mm_iter));
  }

  if (gcell_size.has_value()) {
    args.emplace_back("--gcell_size");
    args.emplace_back(fmt::format("{}", *gcell_size));
  }

  return main(args, false);
}

int Banyan::add_placement_blockage(std::vector<std::string> args) {
  args.emplace_back("--techlef");
  args.emplace_back(tech_lef_file_name_);
  for (const auto& lef : other_lef_file_names_) {
    args.emplace_back("--lef");
    args.emplace_back(lef);
  }
  for (const auto& lef : other_tech_lef_file_names_) {
    args.emplace_back("--lef");
    args.emplace_back(lef);
  }
  auto defFilepath = genTempFilePath(logger_, ".def");
  ord::OpenRoad::openRoad()->writeDef(defFilepath.c_str(), "5.8");
  args.push_back("--def");
  args.push_back(defFilepath);
  auto outDefFilepath = genTempFilePath(logger_, ".def");
  args.push_back("--out");
  args.push_back(outDefFilepath);
  defer _(nullptr, [&](...) {
      logger_->info(utl::BY, 1, "Writing banyan result {} to eden ...", outDefFilepath);
      loaddef(outDefFilepath);
  });
  return exec(logger_, args,BANYAN_AUTO_APB);
}

void Banyan::remove_placement_blockage() {
  auto block = this->db_->getChip()->getBlock();
  block->deletePlacementBlockages();
}


int Banyan::route_clock(std::string_view max_layer, std::string_view outDb) {
  std::vector<std::string> args;
  args.push_back("route_clock");
  args.push_back("--log_level");
  args.push_back("info");
  args.push_back("--max_layer");
  args.emplace_back(max_layer);
  args.push_back("--save_db");
  args.emplace_back(outDb);
  return main(args, false);
}

int Banyan::optimizeCongestion(std::string_view max_layer) {
  std::filesystem::path root_path = std::filesystem::path(std::string{DEBLAIS_ROOT_DIR()});
  if (!std::filesystem::exists(root_path)) {
    if (!std::filesystem::create_directories(root_path)) {
      spdlog::error("Unable to create dir {}.", root_path);
      return 1;
    }
  }

  using namespace banyan::congestion_optimizer;
  
  // write lef files
  std::vector<std::filesystem::path> lefs;
  lefs.push_back(std::filesystem::absolute(tech_lef_file_name_));
  for (auto const& lef: other_lef_file_names_) {
    lefs.push_back(std::filesystem::absolute(lef));
  }

  // write def files
  std::filesystem::path def = root_path / "iter_0.def";
  ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
  ord.writeDef(def.c_str(), "5.8");
  

  // define write_congestion_data
  std::filesystem::path outdb = root_path / "out.db";
  std::filesystem::path congestion_data = root_path / "iter_0.pb";

  // define path of output def 
  std::filesystem::path out_def = root_path / "iter_1.pbdef";
  std::filesystem::path out_inflation = root_path / "iter_1.inflation";

  DPConfig dp_cfg = {
    lefs: lefs, 
    def: def, 
    congestion_data: congestion_data, 
    inflation_data: std::optional<std::filesystem::path>(), 
    out_def: out_def,
    out_inflation: out_inflation,
  };

  Config cfg = Config(dp_cfg, max_layer, root_path);
  Optimizer optimizer = banyan::congestion_optimizer::Optimizer(cfg, this);
  optimizer.optimize();
  try {
    optimizer.write_back_best();
  } catch(std::exception& e) {
    spdlog::error("Congestion Optimizer Failed. Leave Def File Unchanged.");
    return 2;
  };
  return 0;
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
  std::ifstream outfs(pbDefFilePath);
  if(!outfs.is_open() || !pbdef.ParseFromIstream(&outfs)) {
    logger_->error(utl::BY, 6, "Parse banyan pbdef {} fail", pbDefFilePath);
  }
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
  std::ifstream outfs(dbflie);
  if(!outfs.is_open() || !bycase.ParseFromIstream(&outfs)) {
    logger_->error(utl::BY, 6, "Parse banyan db {} fail", dbflie);
  }
  outfs.close();
  loaddef(pbdef2def(bycase.design()));
}

void Banyan::loaddef(std::string deffile)
{
  auto* blk = db_->getChip()->getBlock();
  blk->deleteBlockages();
  if (blk->getGCellGrid() != nullptr) {
    blk->getGCellGrid()->resetGrid();
  }
  odb::defin(db_, logger_).replaceWires(blk, deffile.c_str());
}

void Banyan::loadRoutingCongestion(std::string path) 
{
	std::ifstream report(path);
	if (!report.is_open()) {
		logger_->error(utl::BY, 7, "Can't Open RoutingCongestion File: {}", path);
	}
	const std::regex gcell_info("Layer:(.*?) Panel:(.*?) cell:(.*?) capacity:(.*?) usage:(.*?)");
	int line_number =0;
	auto grid = db_->getChip()->getBlock()->getGCellGrid();
  grid->resetCongestionMap();
	while (!report.eof()){
		std::string line;
		std::smatch base_match;
		line_number++;
		std::getline(report,line);
		if(line.empty()) {
			continue;
		}
		if(std::regex_match(line,base_match,gcell_info)) {
			auto tech = db_->getTech();
			auto layer = tech->findLayer(base_match[1].str().c_str());
			if (layer == nullptr) {
				logger_->error(utl::BY,9,"not found Layer {}",base_match[1]);
			}
			auto panel = atoi(base_match[2].str().c_str());
			auto cell = atoi(base_match[3].str().c_str());
			auto cap = atof(base_match[4].str().c_str());
			auto usage = atof(base_match[5].str().c_str());
			if(layer->getDirection() == odb::dbTechLayerDir::VERTICAL) {
				grid->setCapacity(layer,panel,cell,uint8_t(cap));
				grid->setUsage(layer,panel,cell,uint8_t(usage));		
			}else{
				grid->setCapacity(layer,cell,panel,uint8_t(cap));
				grid->setUsage(layer,cell,panel,uint8_t(usage));		
			}
		}else{
			logger_->error(utl::BY,8,"Unable to parse line as (line: {}): {}",line_number,line);
		}
	}
  ord::OpenRoad::openRoad()->getGlobalRouter()->updateHeatmap();
}

bool sepratePin(odb::dbITerm* iterm, const lrl::STANetwork& network, const std::shared_ptr<lrl::AutoRecordModifier>& modifier, utl::Logger* logger) {
  FASSERT(network.dbType(iterm) == odb::dbITermObj);
  using lrl::Pin;
  using lrl::Net;
  Pin* p = network.dbConvert<Pin>(iterm);
  struct GETBUFFER : public rsz::Resizer {
    using rsz::Resizer::findBuffers;
    using rsz::Resizer::buffer_cells_;
    using rsz::Resizer::bufferDriveResistance;
  };
  GETBUFFER* geter = (GETBUFFER*)(network.resizer());
  geter->findBuffers();
  std::sort(geter->buffer_cells_.begin(), geter->buffer_cells_.end(), [geter](sta::LibertyCell* l, sta::LibertyCell* r) {
    if (l->area() != r->area()) {
      return l->area() < r->area();
    }
    return geter->bufferDriveResistance(l) < geter->bufferDriveResistance(r);
  });
  sta::LibertyCell* buffer = geter->buffer_cells_[0];
  Net* n = network.net(p);
  
  std::vector<Pin*> pins;
  Pin* driver = nullptr;
  auto piter = network.connectedPinIterator(n);
  while (piter->hasNext()) {
    auto pin = piter->next();
    pins.push_back(pin);
    if (network->network()->isDriver(pin)) {
      driver = pin;
    }
  }
  delete piter;

  if (driver == nullptr) {
    logger->info(utl::BY, 15, "Cannot find driver on net {}", network.name(n));
    return false;
  }
  auto driver_loc = network.location(driver);
  // TODO: Insert buffers based on antenna rule calculations rather than brute-force placement on the farthest pin.
  int64_t farthest_dist = odb::Point::manhattanDistance(driver_loc, driver_loc);
  for (auto& pin : pins) {
    if (pin != driver) {
      auto pin_loc = network.location(pin);
      auto pin_dist = odb::Point::manhattanDistance(driver_loc, pin_loc);
      if (pin_dist > farthest_dist) {
        farthest_dist = pin_dist;
      }
    }
  }
  auto threshold = farthest_dist / 2;
  bool inserted = false;
  for (auto& pin : pins) {
    if (pin != driver) {
      auto pin_loc = network.location(pin);
      auto pin_dist = odb::Point::manhattanDistance(driver_loc, pin_loc);
      if (pin_dist >= threshold) {
        odb::Point center(
          (driver_loc.x()+pin_loc.x())/2,
          (driver_loc.y()+pin_loc.y())/2);
        // TODO: Reuse one EcoPlacer instance with modification tracking to avoid repeated reconstruction.
        linden::EcoPlacer eco_placer;
        auto place_iter = linden::PlaceIterator(eco_placer, buffer->name(), center);
        auto valid_loc = place_iter.next();
        if (!valid_loc.has_value()) {
          continue;
        }
        modifier->insertRepeator(buffer, valid_loc.value().point, n, {pin}, valid_loc.value().orient.getValue());
        inserted = true;
      }
    }
  }
  return inserted;
}

void Banyan::incrRoute(
    const std::string& max_layer,
    const std::string& ant_rpt,
    const std::string& save_db,
    const std::string& history_db)
{
  // Banyan Incr Route
  std::vector<std::string> incr_route_args = {
      "incr_route",
      "--log_level", "debug",
      "--max_layer", max_layer,
      "--history", history_db,
      "--fix_ant", ant_rpt,
      "--save_db", save_db,
  };
  this->main(incr_route_args, true);
}

void Banyan::refineRoute(
    const std::string& min_layer,
    const std::string& max_layer,
    const std::string& tmp_dir,
    const std::string& save_db)
{
  // Banyan Refine Route
  std::vector<std::string> eco_args = {
      "refine_route",
      "--log_level", "debug",
      "--min_layer", min_layer,
      "--max_layer", max_layer,
      "--same_net_fixer",
      "--save_db", save_db,
  };
  this->main(eco_args, true);
}
int Banyan::repair_antennas(odb::dbMTerm* diode_mterm,
                            std::string min_layer,
                            std::string max_layer,
                            const int max_iter,
                            const int max_fanout)
{
  auto gr = ord::OpenRoad::openRoad()->getGlobalRouter();
  auto ant_checker = ord::OpenRoad::openRoad()->getAntennaChecker();
  auto num_threads = ord::OpenRoad::openRoad()->getThreadCount();
  auto tmp_dir = genTempDir(logger_);
  auto ant_rpt =  tmp_dir + "repair_antennas.arpt";
  ant_checker->setReportFileName(ant_rpt.c_str());
  auto count = ant_checker->checkAntennas(nullptr,num_threads,false);

  if (count == 0 ){
    return count;
  }
  auto save_db = tmp_dir + "jump_layer.pb";
  std::vector<std::string> fix_antenna_args = {
    "fix_antenna",
    "--log_level","debug",
    "--max_layer",max_layer,
    "--fix_ant",ant_rpt,
    "--save_db", save_db,
  };
  this->main(fix_antenna_args,true);
  ant_rpt = tmp_dir + "jump_layer_before_refine.arpt";
  ant_checker->setReportFileName(ant_rpt.c_str());
  count = ant_checker->checkAntennas(nullptr,num_threads,false);
  refineRoute(min_layer, max_layer, tmp_dir, save_db);
  auto last_insert_diode = 0;
  auto last_total_insert_diode = 0;
  auto history_db = save_db;
  ant_rpt = tmp_dir + "jump_layer.arpt";
  ant_checker->setReportFileName(ant_rpt.c_str());
  count = ant_checker->checkAntennas(nullptr,num_threads,false);
  for(auto iter = 0;iter < max_iter; iter++) {
    logger_->info(utl::BY,9,"Run repair_antennas pre:{}",count);
    history_db = save_db;
    save_db = fmt::format("{}incr_route_{}.pb",tmp_dir,iter);
    auto total_insert_diode = gr->repairAntennas(diode_mterm,1,0,num_threads,max_fanout);
    auto insert_diode = total_insert_diode - last_total_insert_diode;
    if (!insert_diode && !last_insert_diode) {
      count = ant_checker->checkAntennas(nullptr,num_threads,false);
      break;
    }
    last_insert_diode = insert_diode;
    last_total_insert_diode = total_insert_diode;
    incrRoute(max_layer, ant_rpt, save_db, history_db);
    ant_rpt = fmt::format("{}incr_route_{}.arpt",tmp_dir,iter);
    ant_checker->setReportFileName(ant_rpt.c_str());
    count = ant_checker->checkAntennas(nullptr,num_threads,false);
    if (insert_diode > 0 && count > 0 && iter != max_iter-1  ) {
      continue ;
    }
    save_db = fmt::format("{}refine_route_{}.pb", tmp_dir, iter);
    refineRoute(min_layer, max_layer, tmp_dir, save_db);
    ant_rpt = fmt::format("{}refine_route_{}.arpt",tmp_dir,iter);
    ant_checker->setReportFileName(ant_rpt.c_str());
    count = ant_checker->checkAntennas(nullptr,num_threads);
    logger_->info(utl::BY,10,"Run repair_antennas after:{}",count);
    if(count == 0) {
      break ;
    }
  }

  if (count > 0) { // try insert buffers
    for (auto iter = 0; count > 0 && iter < max_iter; iter++) {
      logger_->info(utl::BY,11,"Remaining antenna violation count {}, trying insert buffer near pins", count);
      lrl::History lrl_history;
      lrl::STANetwork lrlnw(ord::OpenRoad::openRoad());
      auto modifier = lrl_history.modifier(lrlnw);
      auto net_vios = ant_checker->getAllAntennaViolations(nullptr, 0);
      std::unordered_set<odb::dbITerm*> pins;
      for (auto& net_vio : net_vios) {
        auto& vios = net_vio.second;
        for (auto& vio : vios) {
          for (auto& gate : vio.gates) {
            pins.insert(gate);
            auto pin_type = lrlnw.dbType(gate);
            FASSERT(pin_type == odb::dbITermObj);
          }
        }
      }
      for (auto& gate : pins) {
        if (!sepratePin(gate, lrlnw, modifier, logger_)) {
          logger_->info(utl::BY,12,"Can't insert buffer for pin {}",gate->getName());
        } else {
          logger_->info(utl::BY,13,"Insert buffer for pin {}",gate->getName());
        }
      }
      incrRoute(max_layer, ant_rpt, save_db, history_db);
      ant_rpt = fmt::format("{}incr_route_after_insert_buffer_{}.arpt",tmp_dir, iter);
      ant_checker->setReportFileName(ant_rpt.c_str());
      count = ant_checker->checkAntennas(nullptr,num_threads,false);
      save_db = fmt::format("{}refine_route_after_insert_buffer_{}.pb", tmp_dir, iter);
      if (count == 0 || iter == max_iter-1) {
        refineRoute(min_layer, max_layer, tmp_dir, save_db);
        ant_rpt = fmt::format("{}refine_route_after_insert_buffer_{}.arpt",tmp_dir, iter);
        ant_checker->setReportFileName(ant_rpt.c_str());
        count = ant_checker->checkAntennas(nullptr,num_threads);
        logger_->info(utl::BY,14,"Run repair_antennas after:{}",count);
      }
      if (count == 0) {
        break;
      }
    }
  }
  return count;
}

void Banyan::newIncrementalRoute(std::string max_layer, std::string work_dir, std::string init_db)
{
  incr_route = new BanyanIncrRoute(this, max_layer, work_dir, init_db);
}

int Banyan::incrementalRoute()
{
  if (incr_route == nullptr) {
    logger_->error(utl::BY, 13, "plz call 'incrementalRouteBegin' cmd first");
  }
  return incr_route->incrementalRoute();
}

void Banyan::deleteIncrementalRoute()
{
  delete incr_route;
  incr_route = nullptr;
}
std::vector<std::string_view> getStringViews(const std::vector<std::string>& names) {
    std::vector<std::string_view> res;
    res.reserve(names.size());
    for (const auto& name : names) {
      res.emplace_back(name);
    }
    return res;
}
std::string_view Banyan::getTechlef() {
    return std::string_view(this->tech_lef_file_name_);
}

std::vector<std::string_view> Banyan::getOtherTechlefs() {
    return getStringViews(this->other_tech_lef_file_names_);
}

std::vector<std::string_view> Banyan::getLiblefs() {
    return getStringViews(this->other_lef_file_names_);
}

}  // namespace banyan
