#pragma once
#include "pprint.hpp"

namespace banyan {
extern pprint::PrettyPrinter printer;

enum class BanyanLogLevel
{
  TRACE,
  DEBUG,
  INFO
};

struct BanyanOptions
{
  std::string debugOutput;
  BanyanLogLevel logLevel;
  bool debugSaveTxt;
  std::string minLayer;
  std::string maxLayer;
  uint32_t maxViaStack;
  bool cts;
  bool disableRoute;
  bool disableMazeTop;
  bool disablePAMcf;
  bool babyBanyan;
  std::string reportRouteJson;
  std::string reportBenchmarkJson;
  bool timingDriven;
  bool dryrun;
};

extern BanyanOptions gOptions;
}  // namespace banyan