#pragma once

#include <ortools/graph/min_cost_flow.h>
#include <ortools/linear_solver/linear_solver.h>

#include "lrl/Options.hh"
#include "rsz/Resizer.hh"
#include "sta/ArcDelayCalc.hh"
#include "sta/Bfs.hh"
#include "sta/Corner.hh"
#include "sta/DcalcAnalysisPt.hh"
#include "sta/FuncExpr.hh"
#include "sta/Fuzzy.hh"
#include "sta/Graph.hh"
#include "sta/GraphDelayCalc.hh"
#include "sta/InputDrive.hh"
#include "sta/Liberty.hh"
#include "sta/Network.hh"
#include "sta/Parasitics.hh"
#include "sta/PathExpanded.hh"
#include "sta/PathRef.hh"
#include "sta/PathVertex.hh"
#include "sta/PortDirection.hh"
#include "sta/Report.hh"
#include "sta/Sdc.hh"
#include "sta/Search.hh"
#include "sta/SearchPred.hh"
#include "sta/StaMain.hh"
#include "sta/TimingArc.hh"
#include "sta/TimingModel.hh"
#include "sta/TimingRole.hh"
#include "sta/Units.hh"

namespace lrl {

using namespace operations_research;
using namespace sta;
using namespace rsz;

using operations_research::MPSolver;
using rsz::Resizer;

typedef int32_t Index;
typedef int64_t Cost;
typedef std::string Name;

typedef std::pair<Name, Name> WirePair;
typedef sta::Set<WirePair> WirePairSet;
typedef sta::Map<Name, MPVariable*> VertexMPVarMap;
typedef sta::Map<Sdc*, VertexMPVarMap*> MMVertexMPVarMap;
typedef sta::Map<WirePair, MPVariable*> WireMPVarMap;
typedef sta::Map<Index, MPConstraint*> MPConstraintMap;
typedef sta::Vector<Name> VertexNameSeq;
typedef sta::Set<Name> VertexNameSet;

typedef sta::Vector<Index> IndexSeq;
typedef sta::Vector<Cost> CostSeq;
typedef std::pair<Index, Index> IdEdge;
typedef std::pair<std::pair<Index, Index>, Delay> IdEdgeWeight;
typedef sta::Set<IdEdgeWeight> IdEdgeWeightSet;

typedef MPSolver::ResultStatus HoldResult;
typedef int32_t SetupResult;

class LaurelAdapter : public Resizer
{
 protected:
  // common methods
  virtual VertexSet getAllRelatedEnds(VertexSet ends, bool ignore_clk);
  virtual Vertex* findRealVertex(Name vertex_name) const;
  virtual Pin* findFanoutPin(WirePair wire_pair) const;

  // constants
  const char* LRL_H_SOLVER = "SCIP";
  const float LRL_H_INFINITY = (std::numeric_limits<float>::max)();
  const float LRL_H_ZERO = 0.0f;
  const int32_t LRL_H_SCALING = 100000;
  const int32_t LRL_H_PRIORITY = 10000000;
  const int32_t LRL_H_STA_SCALING = 1000000000;
  const float LRL_H_ROUNDING_LIMIT = 0.005f / LRL_H_STA_SCALING;
  const int32_t LRL_H_BUFFER_SPILIT = 2;

  const int32_t LRL_S_INT_INF = (std::numeric_limits<int32_t>::max)();
  const int32_t LRL_S_INVALIDE = -1;
  const int32_t LRL_S_ZERO = 0;
  const char* LRL_S_SRC_NAME = "$src$";
  const Index S_INDEX = LRL_S_ZERO;
};

class SetupFixer : public LaurelAdapter
{
 public:
  SetupFixer(OpenRoad* openroad, Options* options);
  virtual ~SetupFixer(){};

  virtual void initAlgorithm();
  SetupResult resolve();
  bool remap();
  Delay getNewDelay(Vertex* vertex);
  Delay getResizeValue(Vertex* vertex);
  VertexSet getResizeVertex();

  void report();

 protected:
  virtual void buildIndexMap(VertexSet ends);
  Index buildIndex(Vertex* vertex);
  void buildAllFaninEdge(VertexSeq fanins);
  void buildALLEdgePO(VertexSet ends);

  virtual Delay costSlope(Vertex* pred, Vertex* succ);

  void addConstraintBase(Index pred, Index succ, Cost capacity, Cost cost);
  virtual void addConstraintEdge(Vertex* pred, Vertex* succ);
  virtual void addConstraintPO(Vertex* po);
  virtual void addConstraintPI(Vertex* pi);

  IdEdgeWeightSet buildBellmanGraph() const;
  void initArrivalMap();
  bool updateArrivalMap(const IdEdgeWeightSet bellman_graph);

  Cost delay2Cost(Delay delay);
  Delay cost2Delay(Cost cost) const;
  bool isInf(Cost cost) const { return cost == LRL_S_INT_INF; }

  Index getIndex(Vertex* vertex) const;
  Name getNodeVertexName(Index index) const;
  Delay getDelay(Vertex* vertex);
  const char* getInPortName(Vertex* pred, Vertex* succ) const;
  Delay getBound(Vertex* vertex, const MinMax* min_max);
  Delay getLowBound(Vertex* vertex);
  Delay getHighBound(Vertex* vertex);

  VertexSeq sortVertexSet(VertexSet set) const;

  // variables
  Index index_ = S_INDEX;

  SimpleMinCostFlow solver_;
  IndexSeq start_nodes_;
  IndexSeq end_nodes_;
  CostSeq capacities_;
  CostSeq unit_costs_;

  sta::Map<Index, Arrival> arrival_map_;

  sta::Map<Name, Index> vertex_index_map_;

  Options* options_;
};

class WireBudgeting : public SetupFixer
{
 public:
  WireBudgeting(OpenRoad* openroad, Options* options)
      : SetupFixer(openroad, options){};
  virtual ~WireBudgeting(){};

  virtual void initAlgorithm();

  uint64 getNewWireLength(Vertex* pred,
                          Vertex* succ,
                          uint64& max_length,
                          Delay& bud_rate);
  WirePairSet getAllWirePairs() { return opt_wires_; }
  bool hasWire(WirePair wire);

 protected:
  virtual void buildIndexMap(VertexSet ends) override;

  virtual Delay costSlope(Vertex* pred, Vertex* succ) override;
  Delay getLowBound(Vertex* pred, Vertex* succ);
  Delay getHighBound(Vertex* pred, Vertex* succ);
  virtual void addConstraintEdge(Vertex* pred, Vertex* succ) override;
  virtual void addConstraintPO(Vertex* po) override;
  virtual void addConstraintPI(Vertex* pi) override;

  uint64 getTreeDist(Vertex* drvr, Vertex* dst);
  SteinerPt findNextPt(SteinerTree* tree, SteinerPt drvr_pt, SteinerPt last);

  // variable
  WirePairSet opt_wires_;
};

class HoldFixer : public LaurelAdapter
{
 public:
  HoldFixer(OpenRoad* openroad, Options* options);
  virtual ~HoldFixer();

  HoldResult resolve();
  Delay getWirePadding(WirePair wire_pair) const;
  WirePairSet getAllWirePairFrom(Name vertex_name) const;
  VertexNameSet getNonZeroPaddingPred() const;

 protected:
  void makeVar(Name vertex_name, MMVertexMPVarMap& arrival);
  void makeWire(Name pred, Name succ);
  void makeVarFanins(VertexSeq fanins);
  void makeVarAllModes();
  void makeDelay(Vertex* vertex);
  void findHoldVioNameMM();

  int addConstraints(Delay low_bound, Delay high_bound);
  void genPIConstraints();
  void genPOConstraints(VertexSet* ends);
  void genWireConstraints(WirePair wire);

  Delay getArrivalMax(Vertex* vertex) const;
  Delay getDelay(Name vertex_name) const;
  Delay getCriLongArrival(Vertex* end) const;

  bool hasHoldViolation(Vertex* vertex) const;
  VertexSet getHoldViolations();  // should run findHoldVioNaemMM before

  VertexNameSeq getSortedNameSeq(VertexMPVarMap* mp_var_map);

  // can not do lp for 10^-10, should zoom in
  Delay zoomIn(Delay ori) const { return ori * LRL_H_SCALING; }
  Delay zoomOut(Delay ori) const { return ori / LRL_H_SCALING; }

  bool isPI(Name vertex_name) const;

  // variables
  int ct_index_ = 0;
  int var_index_ = 0;
  Slack sdc_worst_slack_ = LRL_H_ZERO;

  MPSolver* solver_;
  MMVertexMPVarMap arrival_max_;  // A
  MMVertexMPVarMap arrival_min_;  // a
  WireMPVarMap wire_;             // w
  MPConstraintMap constraint_;

  sta::Map<Name, Delay> delay_map_;
  VertexNameSet vio_ends_name_;
  VertexNameSet complete_vio_ends_;

  Options* options_;
};

class Laurel : public LaurelAdapter
{
 public:
  Laurel() : LaurelAdapter() { options_ = new Options(); }
  virtual ~Laurel();

  void initLaurel(OpenRoad* openroad,
                  Tcl_Interp* interp,
                  Logger* logger,
                  Gui* gui,
                  dbDatabase* db,
                  dbSta* sta,
                  SteinerTreeBuilder* stt_builder,
                  GlobalRouter* global_router);

  void fixHold();
  void fixSetup();
  void wireBudgeting();

  // get Wire budgeting result by pin to pin after wireBudgeting()
  // unit DBU
  uint64 getWireBudMaxLen(Vertex* fanin, Vertex* fanout);
  uint64 getWireBudMaxLen(Pin* fanin, Pin* fanout);
  Delay getWireBudRate(Pin* fanin, Pin* fanout);

  Options* getOptions() { return options_; }

 protected:
  bool fixHoldCore();

  int findInsertBufferCell(Delay max_padding, LibertyCell* buffer_cell);
  bool insertBuffer(HoldFixer* fixer, Name vertex_name);
  bool insertAllBuffer(HoldFixer* fixer);
  void insertHoldDelay(Vertex* drvr,
                       int buffer_count,
                       PinSeq& load_pins,
                       bool loads_have_out_port,
                       LibertyCell* buffer_cell);

  LibertyCell* findHoldBuffer(Delay min_limit);

  LibertyCell* findResizeCell(Vertex* vertex, Delay target_delay);
  bool resizeCell(Vertex* vertex, Delay target_delay);
  bool resizeAllCells(SetupFixer* fixer);

  // bool genAllWireBud(WireBudgeting* guider);
  void showAllWireBud(WireBudgeting* guider);  // debug

  // variable
  Options* options_;
  // sta::Map<WirePair, uint64> wire_budget_;
  // sta::Map<WirePair, uint64> wire_budget_rate_;
  WireBudgeting* wire_bud_ = nullptr;
};

}  // namespace lrl