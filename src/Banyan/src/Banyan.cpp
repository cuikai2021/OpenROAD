#include "banyan/Banyan.h"

namespace sta {
// Tcl files encoded into strings.
extern const char* Banyan_tcl_inits[];
}  // namespace sta

extern "C" {
extern int Banyan_Init(Tcl_Interp* interp);
}

namespace banyan {

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
  const char BANYAN_EXE_DEFAULT[] = "/usr/local/bin/banyan";
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

int exec(utl::Logger* logger, std::vector<std::string> args)
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
}

int Banyan::main(std::vector<std::string> args)
{
  bool debugActive = false;
#ifndef NDEBUG
  debugActive = true;
#endif
  OpenDBAdapter dbAdapter(
      debugActive, db_, stt_builder_, logger_, laurel_, false);

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

  std::string outPbFilePath = genTempFilePath(logger_, ".pb");
  args.push_back("--output");
  args.push_back(outPbFilePath);

  auto status = exec(logger_, args);

  logger_->info(utl::BY, 1, "Writing banyan result to eden ...");
  BanyanResult byrst;
  std::ifstream outfs;
  outfs.open(outPbFilePath);
  byrst.ParseFromIstream(&outfs);
  outfs.close();

  dbAdapter.updateDB(byrst);
  return status;
}

int Banyan::cmap(std::deque<InstanceData> gpInsts, std::string cmapPb)
{
  auto odbInst2Snap = [](odb::dbInst* inst) -> InstanceData {
    InstanceData snap;
    snap.name = inst->getName();
    int orix = 0;
    int oriy = 0;
    inst->getOrigin(orix, oriy);
    snap.x = orix;
    snap.y = oriy;
    return snap;
  };

  std::deque<InstanceData> snapshot;
  auto chip = db_->getChip();
  assert(chip != nullptr);
  auto block = chip->getBlock();
  assert(block != nullptr);

  for (auto inst : gpInsts) {
    auto odbInst = block->findInst(inst.name.c_str());
    assert(odbInst != nullptr);
    snapshot.emplace_back(odbInst2Snap(odbInst));
    odbInst->setOrigin(inst.x, inst.y);
  }

  std::vector<std::string> args;
  args.push_back("--log_level");
  args.push_back("info");
  args.push_back("--report_cmap");
  args.push_back(cmapPb);
  args.push_back("--disable_maze_top");
  args.push_back("true");
  args.push_back("--baby_banyan");
  args.push_back("true");
  main(args);

  for (auto inst : snapshot) {
    auto odbInst = block->findInst(inst.name.c_str());
    assert(odbInst != nullptr);
    odbInst->setOrigin(inst.x, inst.y);
  }

  return 0;
}

}  // namespace banyan
