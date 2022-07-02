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
  bool debugOutputUsed;
  std::string debugOutput;
  BanyanLogLevel logLevel;
  bool debugSaveTxt;
  std::string minLayer;
  std::string maxLayer;
  uint32_t maxViaStack;
  bool cts;
  bool enableRouteUsed;
  bool enableRoute;
  bool enableMazeTopUsed;
  bool enableMazeTop;
  bool babyBanyan;
  bool paMcf;
  bool reportRouteJsonUsed;
  std::string reportRouteJson;
  bool reportBenchmarkJsonUsed;
  std::string reportBenchmarkJson;
  bool dryrun;
};

extern BanyanOptions gOptions;
}  // namespace banyan