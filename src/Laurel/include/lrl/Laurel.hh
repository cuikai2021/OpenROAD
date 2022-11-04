#pragma once

#include "lrl/LaurelAdapter.hh"

namespace lrl {

class SetupFixer;
class ClockConcurrentFixer;
class WireBudgeting;
class HoldBuffering;

class Laurel : public LaurelAdapter
{
 public:
  Laurel() : LaurelAdapter(){ options_ = std::make_shared<Options>(); }
  void deleteLaurel();

  void initLaurel(OpenRoad* openroad,
                  Tcl_Interp* interp,
                  Logger* logger,
                  Gui* gui,
                  dbDatabase* db,
                  dbSta* sta,
                  SteinerTreeBuilder* stt_builder,
                  GlobalRouter* global_router);

  int fixHold(bool multi_mode, bool partial);
  int fixSetup(int iter);
  int ccopt();
  int fastOpt(int iter);
  void wireBudgeting();
  void wireBudgeting(float factor);
  void wireBudgetingSpef(const char* filename);
  void wireBudgetingRC();

  void reportViolations(MinMax const* min_max, bool ignore_top_ports);

  // easy to get the hold slack
  Slack getHoldSlack(Vertex* drvr);
  Slack getHoldSlack(Pin* drvr_pin);

  // get Wire budgeting result by pin to pin after wireBudgeting()
  // unit DBU
  uint64 getWireBudMaxLen(Vertex* fanin, Vertex* fanout);
  uint64 getWireBudMaxLen(Pin* fanin, Pin* fanout);
  Delay getWireBudRate(Vertex* fanin, Vertex* fanout);
  Delay getWireBudRate(Pin* fanin, Pin* fanout);

  // get useful skew (default is zero) after ccopt();
  float usefulSkew(Vertex const* vertex) const;

  Options* getOptions() { return options_.get(); }

 protected:
  // if vertex v is nullptr, fix hold in global mode
  bool fixHoldCore(bool multi_mode, HoldBuffering* b, VertexSet* v = nullptr);

  bool checkVertexBudgeted(Vertex* pred, Vertex* succ);
  void showAllWireBud(WireBudgeting* guider);  // debug
  void wireBudgetingCore();

  void initFastOpt(OpenRoad* openroad);
  // bool quickFixSetup(Vertex* end);
  bool quickFixSetup(Vertex* v);
  void splitFanoutLoads(PathRef* drvr_path);

  bool isWorstPathFromInPort(Vertex* vertex, MinMax const* min_max);

  // variable
  int infeasible_ = 0;
  std::shared_ptr<Options> options_;

  WireBudgeting* wire_bud_ = nullptr;
  ClockConcurrentFixer* ccopt_fixer_ = nullptr;
};

}  // namespace lrl