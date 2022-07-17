#pragma once

#include <ortools/graph/min_cost_flow.h>
#include <ortools/linear_solver/linear_solver.h>

#include <unordered_map>

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

typedef std::pair<Index, Index> WirePair;
typedef sta::Set<WirePair> WirePairSet;
typedef std::unordered_map<Index, MPVariable*> VertexMPVarMap;
typedef sta::Map<Sdc*, std::shared_ptr<VertexMPVarMap>> MMVertexMPVarMap;
typedef sta::Map<WirePair, MPVariable*> WireMPVarMap;
typedef sta::Map<Index, MPConstraint*> MPConstraintMap;
typedef sta::Set<Index> VertexIndexSet;

typedef sta::Vector<Index> IndexSeq;
typedef sta::Vector<Cost> CostSeq;
typedef std::pair<Index, Index> IdEdge;
typedef std::pair<std::pair<Index, Index>, Delay> IdEdgeWeight;
typedef sta::Set<IdEdgeWeight> IdEdgeWeightSet;

typedef std::pair<Vertex*, Vertex*> VertexPair;
typedef sta::Set<VertexPair> VertexPairSet;

typedef MPSolver::ResultStatus HoldResult;
typedef int32_t SetupResult;

class LaurelAdapter : public Resizer
{
 protected:
  // common methods
  virtual void getAllRelatedEnds(VertexSet ends,
                                 bool ignore_clk,
                                 VertexSet& related_ends);
  virtual Delay getArrivalMax(Vertex* vertex) const;

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
  const char* LRL_S_SNK_NAME = "$snk$";
  const Index S_INDEX = LRL_S_ZERO;
  const Index T_INDEX = S_INDEX + 1;
};

class SetupFixer : public LaurelAdapter
{
 public:
  SetupFixer(OpenRoad* openroad, Options* options);
  virtual ~SetupFixer(){};

  virtual void initAlgorithm();
  SetupResult resolve();
  bool remap();
  void getResizeQueue(sta::Map<Vertex*, LibertyCell*>& queue);

  void report();

 protected:
  virtual void buildIndexMap(const VertexSet& ends);
  Index buildIndex(Vertex* vertex);
  virtual void buildAllFaninEdge(const VertexSet& fanins);
  virtual void buildALLEdgePrimeOut(const VertexSet& ends);

  virtual Delay costSlope(Vertex* pred, Vertex* succ);

  void addConstraintBase(Index pred, Index succ, Cost capacity, Cost cost);
  virtual void addConstraintEdge(Vertex* pred, Vertex* succ);
  virtual void addConstraintPrimeOut(Vertex* po);
  virtual void addConstraintPrimeIn(Vertex* pi);

  void buildBellmanGraph(IdEdgeWeightSet& bellman_graph);
  void initArrivalMap();
  bool updateArrivalMap(const IdEdgeWeightSet& bellman_graph);

  Cost delay2Cost(Delay delay);
  Delay cost2Delay(Cost cost) const;
  bool isInf(Cost cost) const { return cost == LRL_S_INT_INF; }

  Index getIndex(Vertex* vertex) const;
  Delay getDelay(Vertex* vertex);
  virtual Delay getCellDelay(Vertex* vertex);
  Delay getBound(Vertex* vertex, const MinMax* min_max);
  Delay getLowBound(Vertex* vertex);
  Delay getHighBound(Vertex* vertex);

  LibertyCell* findResizeCell(Vertex* vertex, Delay target_delay);
  Delay getNewDelay(Vertex* vertex);
  Delay getResizeValue(Vertex* vertex);
  void getResizeVertex(VertexSet& resize_vertex);

  // variables
  Index index_ = T_INDEX + 1;

  SimpleMinCostFlow solver_;
  IndexSeq start_nodes_;
  IndexSeq end_nodes_;
  CostSeq capacities_;
  CostSeq unit_costs_;

  int bellman_max_iter_num_ = 0;

  sta::Map<Index, Arrival> arrival_map_;
  sta::Map<Index, Arrival> pi_arrival_map_;

  sta::Map<Vertex*, Index> vertex_index_map_;
  sta::Map<Vertex*, Delay> vertex_high_bound_map_;
  sta::Map<Vertex*, Delay> vertex_low_bound_map_;
  sta::Set<Index> pi_index_set_;

  Options* options_;
};

class WireBudgeting : public SetupFixer
{
 public:
  WireBudgeting(OpenRoad* openroad, Options* options)
      : SetupFixer(openroad, options){};
  virtual ~WireBudgeting(){};

  virtual void initAlgorithm();

  bool makeDelaySpef() { return makeDelaySpefCore(spef_delay_, 1.0); }
  bool makeDelaySpefFct() { return makeDelaySpefCore(spef_delay_factor_, 2.0); }

  virtual void getNewWireLength(Vertex* pred,
                                Vertex* succ,
                                uint64& max_length,
                                Delay& bud_rate);
  VertexPairSet& getAllWirePairs() { return opt_wires_; }

  bool hasWire(const VertexPair& wire);
  Vertex* getVertex(Pin* pin) { return pin_vertex_map_.findKey(pin); }

 protected:
  virtual void buildAllFaninEdge(const VertexSet& fanins) override;

  virtual Delay costSlope(Vertex* pred, Vertex* succ) override;
  virtual Delay getLowBound(Vertex* pred, Vertex* succ);
  virtual Delay getHighBound(Vertex* pred, Vertex* succ);
  virtual void addConstraintEdge(Vertex* pred, Vertex* succ) override;

  SteinerTree* getTree(Vertex* drvr);
  uint64 getTreeDist(Vertex* drvr, Vertex* dst);
  SteinerPt findNextPt(SteinerTree* tree,
                       const SteinerPt& drvr_pt,
                       const SteinerPt& last);
  Vertex* getRealFanoutVertex(const VertexPair& wire);

  void buildSpefDelayMap(sta::Map<Vertex*, Delay>& spef_delay_map);
  bool makeDelaySpefCore(sta::Map<Vertex*, Delay>& map, double factor);

  // variable
  VertexPairSet opt_wires_;
  sta::Map<Vertex*, SteinerTree*> drvr_tree_map_;
  sta::Map<VertexPair, Vertex*> wire_fanout_map_;
  sta::Map<Pin*, Vertex*> pin_vertex_map_;
  sta::Map<VertexPair, uint64> distance_map_;

  sta::Map<Vertex*, Delay> spef_delay_;
  sta::Map<Vertex*, Delay> spef_delay_factor_;
};

class WireBudgetingFake : public WireBudgeting
{
 public:
  WireBudgetingFake(OpenRoad* openroad, Options* options, float factor)
      : WireBudgeting(openroad, options), factor_(factor){};
  virtual ~WireBudgetingFake(){};

  virtual void initAlgorithm() override;
  virtual void getNewWireLength(Vertex* pred,
                                Vertex* succ,
                                uint64& max_length,
                                Delay& bud_rate) override;

 protected:
  virtual void addConstraintPrimeOut(Vertex* po) override;
  virtual void addConstraintPrimeIn(Vertex* pi) override;

  virtual void addConstraintEdge(Vertex* pred, Vertex* succ) override;

  float factor_;
};

class WireBudgetingRC : public WireBudgeting
{
 public:
  WireBudgetingRC(OpenRoad* openroad, Options* options)
      : WireBudgeting(openroad, options){};
  virtual ~WireBudgetingRC(){};

  virtual void initAlgorithm() override;

 protected:
  virtual Delay costSlope(Vertex* pred, Vertex* succ) override;
  virtual Delay getLowBound(Vertex* pred, Vertex* succ) override;
  virtual Delay getHighBound(Vertex* pred, Vertex* succ) override;

  Delay getDrvrSlope();
  virtual Delay getCellDelay(Vertex* vertex) override;

  sta::Map<Vertex*, Delay> delay_map_;
  Delay wire_slope_ = LRL_H_ZERO;
};

class HoldFixer : public LaurelAdapter
{
 public:
  HoldFixer(OpenRoad* openroad, Options* options);

  HoldResult resolve();
  Delay getWirePadding(const WirePair& wire_pair) const;

  bool insertAllBuffer();
  int getInsertedBufferCnt() const { return inserted_buffer_count_; }

 protected:
  typedef sta::Set<Name> VertexNameSet;

  void makeVar(const Index& vertex_index, MMVertexMPVarMap& arrival);
  void makeWire(const Index& pred, const Index& succ);
  void makeVarFanins(const VertexSet& fanins);
  void makeVarAllModes();
  void makeDelay(Vertex* vertex);
  void findHoldVioNameMM(VertexNameSet& vio_ends_name);

  int addConstraints(Delay low_bound, Delay high_bound);
  void genPrimeInConstraints();
  void genPrimeOutConstraints(VertexSet* ends);
  void genWireConstraints(const WirePair& wire);

  Delay getDelay(const Index& vertex_index) const;
  Delay getCriLongArrival(Vertex* end) const;

  void getRealVertexName();
  Vertex* findRealVertex(const Index& vertex_index) const;

  bool hasHoldViolation(Vertex* vertex,
                        const VertexNameSet& vio_ends_name) const;
  // should run findHoldVioNaemMM before
  void getHoldViolations(const VertexNameSet& vio_ends_name,
                         VertexSet& hold_failures);

  // can not do lp for 10^-10, should zoom in
  Delay zoomIn(Delay ori) const { return ori * LRL_H_SCALING; }
  Delay zoomOut(Delay ori) const { return ori / LRL_H_SCALING; }

  void confirmAllPrimeIn();
  bool isPrimeIn(const Index& vertex_index) const;

  // insert buffers
  Pin* findFanoutPin(Vertex* pred, Vertex* succ) const;
  void insertHoldDelay(Vertex* drvr,
                       int buffer_count,
                       PinSeq& load_pins,
                       bool loads_have_out_port,
                       LibertyCell* buffer_cell);

  LibertyCell* findHoldBuffer(Delay min_limit, int& count);
  bool insertBuffer(const Index& vertex_index);
  void getAllWirePairFrom(const Index& vertex_index,
                          WirePairSet& wire_pair_set);
  void getNonZeroPaddingPred(VertexIndexSet& vio_pred_set);

  // variables
  Index ct_index_ = 0;
  Index var_index_ = 0;
  Index vertex_index_ = 0;
  Slack sdc_worst_slack_ = LRL_H_ZERO;

  MPSolver* solver_;
  MMVertexMPVarMap arrival_max_;  // A
  MMVertexMPVarMap arrival_min_;  // a
  WireMPVarMap wire_;             // w
  MPConstraintMap constraint_;

  std::unordered_map<Name, Index> vertex_name_index_map_;
  std::unordered_map<Index, Delay> delay_map_;
  VertexIndexSet complete_vio_ends_;
  std::unordered_map<Index, Vertex*> sdc_id_vertex_map_;
  std::unordered_map<Vertex*, Index> sdc_vertex_id_map_;
  VertexIndexSet pi_set_;

  Options* options_;
};

class Laurel : public LaurelAdapter
{
 public:
  Laurel() : LaurelAdapter() { options_ = std::make_shared<Options>(); }
  virtual ~Laurel();

  void initLaurel(OpenRoad* openroad,
                  Tcl_Interp* interp,
                  Logger* logger,
                  Gui* gui,
                  dbDatabase* db,
                  dbSta* sta,
                  SteinerTreeBuilder* stt_builder,
                  GlobalRouter* global_router);

  int fixHold();
  int fixSetup();
  void wireBudgeting();
  void wireBudgeting(float factor);
  void wireBudgetingSpef(const char* filename);
  void wireBudgetingRC();

  // easy to get the hold slack
  Slack getHoldSlack(Vertex* drvr);
  Slack getHoldSlack(Pin* drvr_pin);

  // get Wire budgeting result by pin to pin after wireBudgeting()
  // unit DBU
  uint64 getWireBudMaxLen(Vertex* fanin, Vertex* fanout);
  uint64 getWireBudMaxLen(Pin* fanin, Pin* fanout);
  Delay getWireBudRate(Vertex* fanin, Vertex* fanout);
  Delay getWireBudRate(Pin* fanin, Pin* fanout);

  Options* getOptions() { return options_.get(); }

 protected:
  bool fixHoldCore();

  bool resizeAllCells(SetupFixer* fixer);

  bool checkVertexBudgeted(Vertex* pred, Vertex* succ);
  void showAllWireBud(WireBudgeting* guider);  // debug
  void wireBudgetingCore();

  // variable
  std::shared_ptr<Options> options_;

  WireBudgeting* wire_bud_ = nullptr;
};

}  // namespace lrl