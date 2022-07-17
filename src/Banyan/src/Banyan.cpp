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

std::vector<banyan_go::option> banyanOptionVec(const BanyanOptions& byOptions)
{
  std::vector<banyan_go::option> vec;
  vec.push_back(
      {"log_level", magic_enum::enum_name(byOptions.logLevel).data()});
  vec.push_back({"debug_output", byOptions.debugOutput.c_str()});
  vec.push_back({"debug_save_txt", byOptions.debugSaveTxt ? "true" : ""});
  vec.push_back({"cts", byOptions.cts ? "true" : ""});
  vec.push_back({"enable_route", byOptions.disableRoute ? "" : "true"});
  vec.push_back({"enable_maze_top", byOptions.disableMazeTop ? "" : "true"});
  vec.push_back({"pa_mcf", byOptions.disablePAMcf ? "" : "true"});
  vec.push_back({"baby_banyan", byOptions.babyBanyan ? "true" : ""});
  vec.push_back({"timing_driven", byOptions.timingDriven ? "true" : ""});
  vec.push_back({"report_route_json", byOptions.reportRouteJson.c_str()});
  vec.push_back(
      {"report_benchmark_json", byOptions.reportBenchmarkJson.c_str()});
  return vec;
}

void printBanyanOptions(const BanyanOptions& byOptions)
{
  printer.print("Banyan options:");
  printer.print("\tdebugOutput:         ", "\t", byOptions.debugOutput);
  printer.print("\tlogLevel:            ",
                "\t",
                magic_enum::enum_name(byOptions.logLevel));
  printer.print("\tdebugSaveTxt:        ", "\t", byOptions.debugSaveTxt);
  printer.print("\tminLayer:            ", "\t", byOptions.minLayer);
  printer.print("\tmaxLayer:            ", "\t", byOptions.maxLayer);
  printer.print("\tmaxViaStack:         ", "\t", byOptions.maxViaStack);
  printer.print("\tcts:                 ", "\t", byOptions.cts);
  printer.print("\tdisableRoute:        ", "\t", byOptions.disableRoute);
  printer.print("\tdisableMazeTop:      ", "\t", byOptions.disableMazeTop);
  printer.print("\tdisablePAMcf:        ", "\t", byOptions.disablePAMcf);
  printer.print("\tbabyBanyan:          ", "\t", byOptions.babyBanyan);
  printer.print("\ttimingDriven         ", "\t", byOptions.timingDriven);
  printer.print("\treportRouteJson:     ", "\t", byOptions.reportRouteJson);
  printer.print("\treportBenchmarkJson: ", "\t", byOptions.reportBenchmarkJson);
  printer.print("\tdryrun:              ", "\t", byOptions.dryrun);
}

int Banyan::main(std::vector<std::string> args)
{
  std::vector<std::string> wrong;

  BanyanCMD selectedCMD = BanyanCMD::HELP;
  {
    gOptions.logLevel = BanyanLogLevel::INFO;
    gOptions.maxViaStack = std::numeric_limits<uint32_t>::max();
  }

  auto argsToStrFn = [](const std::vector<std::string>& vec) -> std::string {
    auto content = std::accumulate(
        vec.begin(),
        vec.end(),
        std::string{},
        [](std::string& s, const std::string& piece) -> decltype(auto) {
          return s += "," + piece;
        });
    boost::replace_first(content, ",", "[");
    content += "]";
    return content;
  };

  auto strToLogLevelFn = [&](const std::string& str) {
    auto castRst = magic_enum::enum_cast<BanyanLogLevel>(str);
    if (!castRst) {
      logger_->error(
          utl::BY, 109, "log_level value must in [TRACE, DEBUG, INFO]");
    }
    gOptions.logLevel = castRst.value();
  };

  auto strToMaxViaStackFn = [&](const std::string& str) {
    std::optional<uint32_t> castRst = getnum(str);

    if (!castRst || castRst.value() <= 0) {
      logger_->error(
          utl::BY, 118, "-max_via_stack value must be an uint32 > 0");
    }
    gOptions.maxViaStack = castRst.value();
  };

  auto printErrorIfRepeatedFn = [&](int argIdx) {
    logger_->info(utl::BY,
                  argIdx,
                  "Args {}, position {} arg match reprated value",
                  argsToStrFn(args),
                  argIdx);
  };
  auto printErrorIfMissing = [&](int argIdx) {
    logger_->info(utl::BY,
                  argIdx,
                  "Args {}, position {} arg missing value",
                  argsToStrFn(args),
                  argIdx);
  };
  auto printErrorIfBlocked = [&](int argIdx) {
    logger_->info(utl::BY,
                  argIdx,
                  "Args {}, position {} arg missing flag",
                  argsToStrFn(args),
                  argIdx);
  };
  auto printErrorIfConflicted = [&](int argIdx) {
    logger_->info(utl::BY,
                  argIdx,
                  "Args {}, position {} arg is conflicted",
                  argsToStrFn(args),
                  argIdx);
  };

  auto helpMode = (command("help").set(selectedCMD, BanyanCMD::HELP));
  auto runMode
      = (command("run").set(selectedCMD, BanyanCMD::RUN),
         option("-debug_output")
             & value("prefix of debugging files", gOptions.debugOutput)
                   .if_repeated(printErrorIfRepeatedFn)
                   .if_missing(printErrorIfMissing)
                   .if_blocked(printErrorIfBlocked)
                   .if_conflicted(printErrorIfConflicted),
         option("-log_level")
             & value("banyan log level")(strToLogLevelFn)
                   .if_repeated(printErrorIfRepeatedFn)
                   .if_missing(printErrorIfMissing)
                   .if_blocked(printErrorIfBlocked)
                   .if_conflicted(printErrorIfConflicted),
         option("-debug_save_txt")
             .set(gOptions.debugSaveTxt, true)
             .if_repeated(printErrorIfRepeatedFn)
             .if_missing(printErrorIfMissing)
             .if_blocked(printErrorIfBlocked)
             .if_conflicted(printErrorIfConflicted),
         option("-min_layer")
             & value("min layer name for routing", gOptions.minLayer)
                   .if_repeated(printErrorIfRepeatedFn)
                   .if_missing(printErrorIfMissing)
                   .if_blocked(printErrorIfBlocked)
                   .if_conflicted(printErrorIfConflicted),
         option("-max_layer")
             & value("max layer name for routing", gOptions.maxLayer)
                   .if_repeated(printErrorIfRepeatedFn)
                   .if_missing(printErrorIfMissing)
                   .if_blocked(printErrorIfBlocked)
                   .if_conflicted(printErrorIfConflicted),
         option("-max_via_stack")
             & value("max via stack")(strToMaxViaStackFn)
                   .if_repeated(printErrorIfRepeatedFn)
                   .if_missing(printErrorIfMissing)
                   .if_blocked(printErrorIfBlocked)
                   .if_conflicted(printErrorIfConflicted),
         option("-dryrun")
             .set(gOptions.dryrun, true)
             .if_repeated(printErrorIfRepeatedFn)
             .if_missing(printErrorIfMissing)
             .if_blocked(printErrorIfBlocked)
             .if_conflicted(printErrorIfConflicted),
         option("-disable_route")
             .set(gOptions.disableRoute, true)
             .if_repeated(printErrorIfRepeatedFn)
             .if_missing(printErrorIfMissing)
             .if_blocked(printErrorIfBlocked)
             .if_conflicted(printErrorIfConflicted),
         option("-disable_maze_top")
             .if_repeated(printErrorIfRepeatedFn)
             .if_missing(printErrorIfMissing)
             .if_blocked(printErrorIfBlocked)
             .if_conflicted(printErrorIfConflicted),
         option("-disable_pa_mcf")
             .if_repeated(printErrorIfRepeatedFn)
             .if_missing(printErrorIfMissing)
             .if_blocked(printErrorIfBlocked)
             .if_conflicted(printErrorIfConflicted),
         option("-cts")
             .set(gOptions.cts, true)
             .if_repeated(printErrorIfRepeatedFn)
             .if_missing(printErrorIfMissing)
             .if_blocked(printErrorIfBlocked)
             .if_conflicted(printErrorIfConflicted),
         option("-baby_banyan")
             .set(gOptions.babyBanyan, true)
             .if_repeated(printErrorIfRepeatedFn)
             .if_missing(printErrorIfMissing)
             .if_blocked(printErrorIfBlocked)
             .if_conflicted(printErrorIfConflicted),
         option("-timing_driven")
             .set(gOptions.timingDriven, true)
             .if_repeated(printErrorIfRepeatedFn)
             .if_missing(printErrorIfMissing)
             .if_blocked(printErrorIfBlocked)
             .if_conflicted(printErrorIfConflicted),
         option("-report_route_json")
             & value("json name of route report", gOptions.reportRouteJson)
                   .if_repeated(printErrorIfRepeatedFn)
                   .if_missing(printErrorIfMissing)
                   .if_blocked(printErrorIfBlocked)
                   .if_conflicted(printErrorIfConflicted),
         option("-report_benchmark_json")
             & value("json name of benchmark report",
                     gOptions.reportBenchmarkJson)
                   .if_repeated(printErrorIfRepeatedFn)
                   .if_missing(printErrorIfMissing)
                   .if_blocked(printErrorIfBlocked)
                   .if_conflicted(printErrorIfConflicted),
         any_other(wrong));

  auto cli = (helpMode | runMode);
  auto mainPage = make_man_page(cli, "banyan_route");

  if (auto parseRst = parse(args.begin(), args.end(), cli);
      !parseRst || !wrong.empty()) {
    for (const auto& arg : wrong) {
      logger_->info(utl::BY, 248, "{} is not a valid argument", arg);
    }
    logger_->error(utl::BY, 250, "{}", mainPage);
  }

  bool debugActive;
#ifndef NDEBUG
  debugActive = true;
#else
  debugActive = gOptions.dryrun;
#endif

  switch (selectedCMD) {
    case BanyanCMD::RUN: {
      auto tech = db_->getTech();
      if (tech == nullptr) {
        logger_->error(
            utl::BY, 261, "Tech rules empty, techlef file is not read");
      }
      if (debugActive) {
        printBanyanOptions(gOptions);
      }
      OpenDBAdapter bdAdapter(db_,
                              stt_builder_,
                              logger_,
                              laurel_,
                              gOptions.maxViaStack,
                              gOptions.minLayer,
                              gOptions.maxLayer,
                              gOptions.timingDriven,
                              debugActive);
      Axioms axioms(bdAdapter, debugActive);

      if (debugActive) {
        bdAdapter.printDB();
      }
      if (!gOptions.dryrun) {
        return run(bdAdapter, axioms);
      }
      break;
    }
    case BanyanCMD::HELP:
      logger_->info(utl::BY, 268, "{}", mainPage);
      break;
  }

  return 0;
}

int Banyan::run(OpenDBAdapter& bdAdapter, Axioms& axioms)
{
  void* so = dlopen("banyan.so", RTLD_NOW);
  if (so == nullptr) {
    logger_->error(
        utl::BY, 280, "cannot load banyan.so: Have you set LD_LIBRARY_PATH?");
    return -1;
  }

  auto banyan_version = (banyan_go::version_func) dlsym(so, "banyan_version");
  if (banyan_version == nullptr) {
    logger_->error(utl::BY, 286, "cannot find banyan_version in banyan.so");
    return -1;
  }
  char interface_version[100];
  sprintf(interface_version,
          "%02x%02x%02x%02x-%02x%02x%02x%02x",
          banyan_go::interface_version_0,
          banyan_go::interface_version_1,
          banyan_go::interface_version_2,
          banyan_go::interface_version_3,
          banyan_go::interface_version_4,
          banyan_go::interface_version_5,
          banyan_go::interface_version_6,
          banyan_go::interface_version_7);
  if (banyan_version(interface_version) != 0) {
    logger_->error(
        utl::BY, 302, "banyan version check failed with {}", interface_version);
    return -1;
  }

  auto banyan_route = (banyan_go::route_func) dlsym(so, "banyan_route");
  if (banyan_route == nullptr) {
    logger_->error(utl::BY, 308, "cannot find banyan_route in banyan.so");
    return -1;
  }

  banyan_go::route_ret ret;
  ::memset(&ret, 0, sizeof(ret));

  auto optionsVec = banyanOptionVec(gOptions);
  long long err = banyan_route(banyan_go::to_slice(optionsVec),
                               bdAdapter.getDesign(),
                               banyan_go::to_slice(bdAdapter.getLayers()),
                               banyan_go::to_slice(bdAdapter.getTracks()),
                               banyan_go::to_slice(bdAdapter.getNets()),
                               banyan_go::to_slice(bdAdapter.getPins()),
                               banyan_go::to_slice(bdAdapter.getBoxes()),
                               banyan_go::to_slice(bdAdapter.getVias()),
                               banyan_go::to_slice(bdAdapter.getViadefs()),
                               banyan_go::to_slice(bdAdapter.getTableTriples()),
                               &ret);

  if (err == 0) {
    bdAdapter.updateDB(bdAdapter.getLayers(),
                       bdAdapter.getNets(),
                       bdAdapter.getViadefs(),
                       ret);
    if (ret.rboxes_size != 0)
      free(ret.rboxes);
    if (ret.rvias_size != 0)
      free(ret.rvias);
    logger_->info(utl::BY, 337, "banyan done");
  } else {
    logger_->info(utl::BY, 339, "banyan error: {}", err);
  }

  return err;
}

}  // namespace banyan
