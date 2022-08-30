#pragma once

#include <ortools/graph/min_cost_flow.h>
#include <ortools/linear_solver/linear_solver.h>

#include <unordered_map>
#include <unordered_set>
#include <boost/bimap.hpp>

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

// macros
#define pathVertex(expanded, i) (expanded.path(i)->vertex(sta_))

#define pathPort(expanded, i) \
  (network_->libertyPort(expanded.path(i)->pin(sta_)))

namespace lrl {

using namespace operations_research;
using namespace sta;
using namespace rsz;

using operations_research::MPSolver;
using rsz::Resizer;

typedef int32_t Index;
typedef int64_t Cost;
typedef std::string Name;

typedef MPSolver::ResultStatus HoldResult;
typedef int32_t SetupResult;

class LaurelAdapter : public Resizer
{
 public:
  // definitions
  typedef std::pair<Index, Index> WirePair;
  typedef sta::Set<WirePair> WirePairSet;
  typedef sta::Set<Index> VertexIndexSet;

  typedef sta::Vector<Index> IndexSeq;
  typedef sta::Vector<Cost> CostSeq;

  typedef std::pair<Vertex*, Vertex*> VertexPair;
  typedef sta::Set<VertexPair> VertexPairSet;
  typedef sta::Map<Vertex*, Delay> VertexDelayMap;

 protected:
  struct VertexHash
  {
    size_t operator()(Vertex* v) const { return (size_t)v; }
  };

  // common methods
  virtual void getAllRelatedEnds(VertexSet ends,
                                 bool ignore_clk,
                                 VertexSet& related_ends);
  virtual Delay getArrival(Vertex* vertex, MinMax const* min_max) const;
  virtual void parasiticPreamble(Resizer* rsz);

  virtual void connectNetLoad(Pin* pin, Net* net);
  virtual void makeBufferInstance(const char* tag,
                                  Point const& location,
                                  LibertyCell* buffer_cell,
                                  Instance*& buffer);

  // constants
  const char* LRL_H_SOLVER = "SCIP";
  static constexpr float LRL_H_INFINITY = (std::numeric_limits<float>::max)();
  static constexpr float LRL_H_ZERO = 0.0f;
  static constexpr int32_t LRL_H_SCALING = 100000;
  static constexpr int32_t LRL_H_PRIORITY = 10000000;
  static constexpr int32_t LRL_H_STA_SCALING = 1000000000;
  static constexpr float LRL_H_ROUNDING_LIMIT = 0.0005f / LRL_H_STA_SCALING;

  static constexpr int32_t LRL_S_INT_INF
      = (std::numeric_limits<int32_t>::max)();
  static constexpr int32_t LRL_S_INVALIDE = -1;
  static constexpr int32_t LRL_S_ZERO = 0;
};

class SetupFixer : public LaurelAdapter
{
 public:
  SetupFixer(OpenRoad* openroad, Options* options);
  virtual ~SetupFixer(){};

  virtual bool initAlgorithm(bool iterative);
  virtual void initAlgorithm() { initAlgorithm(false); }
  SetupResult resolve();
  bool remap();
  void getResizeQueue(sta::Map<Vertex*, LibertyCell*>& queue);

  void report();

 protected:
  // definitions
  enum NodeIndex
  {
    S_INDEX = 0,
    T_INDEX
  };
  struct Constraint
  {
    Index start_node;
    Index end_node;
    Cost capacity;
    Cost unit_cost;
  };
  struct BoundDelay
  {
    Delay high_bound;
    Delay low_bound;
  };
  struct DelayFactor
  {
    Delay current_delay;
    float prev_drive;
    LibertyPort* in_port;
  };

  typedef sta::Vector<Constraint> CstSeq;
  typedef std::pair<Index, Index> IdEdge;
  typedef std::pair<std::pair<Index, Index>, Delay> IdEdgeWeight;
  typedef sta::Set<IdEdgeWeight> IdEdgeWeightSet;

  // methods
  virtual void buildIndexMap(const VertexSet& ends);
  Index buildIndex(Vertex* vertex);
  virtual void buildAllFaninEdge(const VertexSet& fanins);
  virtual void buildALLEdgePrimeOut(const VertexSet& ends);

  virtual Delay costSlope(Vertex* pred, Vertex* succ);

  void addConstraintBase(Constraint&&);
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
  Delay getBound(Vertex* vertex, MinMax const* min_max);
  Delay getLowBound(Vertex* vertex);
  Delay getHighBound(Vertex* vertex);

  LibertyCell* findResizeCell(Vertex* vertex, Delay target_delay);
  Delay getNewDelay(Vertex* vertex);
  Delay getResizeValue(Vertex* vertex);
  void getResizeVertex(VertexSet& resize_vertex);

  // variables
  Index index_ = T_INDEX + 1;

  SimpleMinCostFlow solver_;
  CstSeq constraints_;

  int bellman_max_iter_num_ = 0;

  sta::Map<Index, Arrival> fixed_arrival_map_;

  sta::Map<Vertex*, Index> vertex_index_map_;
  sta::Map<Vertex*, BoundDelay> vertex_bound_map_;
  sta::Map<Vertex*, DelayFactor> vertex_delay_map_;

  Options* options_;
};

class ClockConcurrentFixer : public SetupFixer
{
 public:
  ClockConcurrentFixer(OpenRoad* openroad, Options* options)
      : SetupFixer(openroad, options){};

  virtual void initAlgorithm() override;
  Delay getUsefulSkew(Vertex const* vertex) const;

  void report_skews();

 protected: 

  virtual void addConstraintPrimeOut(Vertex* po) override;
  virtual void addConstraintPrimeIn(Vertex* pi) override;
  void addConstraintPrimeInOut(Vertex* pi, Vertex* po);

  struct Reg2RegSkewMap {
    typedef std::unordered_map<Vertex*, Delay, VertexHash> NodeSkewMap;
    typedef std::unordered_map<Vertex*, std::shared_ptr<NodeSkewMap>, VertexHash> Map;
    void insert(Vertex* i, Vertex* o, Delay const & d) {
      insert_map(i2o, i, o, d);
      insert_map(o2i, o, i, d);
    }
    NodeSkewMap* find(Vertex* i, bool& is_i2o) const {
      is_i2o = true;
      auto it = i2o.find(i);
      if (it == i2o.end()) {
        is_i2o = false;
        it = o2i.find(i);
        if (it == o2i.end()) return NULL;
      }
      return it->second.get();
    } 
  private:
    void insert_map(Map& m, Vertex* k0, Vertex* k1, Delay const& d) {
      auto it = m.find(k0);
      if (it == m.end())
        it = m.insert({k0, std::make_shared<NodeSkewMap>()}).first;
      it->second->insert({k1, d});
    }
    Map i2o;
    Map o2i;
  } opt_regs_;
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

  void buildSpefDelayMap(VertexDelayMap& spef_delay_map);
  bool makeDelaySpefCore(VertexDelayMap& map, double factor);

  // variable
  VertexPairSet opt_wires_;
  sta::Map<Vertex*, SteinerTree*> drvr_tree_map_;
  sta::Map<VertexPair, Vertex*> wire_fanout_map_;
  sta::Map<Pin*, Vertex*> pin_vertex_map_;
  sta::Map<VertexPair, uint64> distance_map_;

  VertexDelayMap spef_delay_;
  VertexDelayMap spef_delay_factor_;
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

  VertexDelayMap delay_map_;
  Delay wire_slope_ = LRL_H_ZERO;
};

class HoldFixer : public LaurelAdapter
{
 public:
  HoldFixer(OpenRoad* openroad, Options* options, bool multi_mode);

  HoldResult resolve();
  Delay getWirePadding(const WirePair& wire_pair) const;

  bool insertAllBuffer();
  int getInsertedBufferCnt() const { return inserted_buffer_count_; }

 protected:
  // definitions
  struct BufferDelay
  {
    LibertyCell* buffer_;
    Delay hold_delay_;
    bool operator<(BufferDelay const& other) const
    {
      return other.hold_delay_ < this->hold_delay_;
    }
  };

  struct VertexIdMap
  {
    typedef boost::bimap<Index, Vertex*> BMap;
    typedef std::unordered_map<Name, Index> Map;
    void clear() { if(!ivb.empty()) ivb.clear(); }
    bool insert(Name const& n, Vertex* v)
    {
      bool new_n = false;
      auto it = ni.find(n);
      if (it == ni.end()) {
        it = ni.insert({n, cur++}).first;
        new_n = true;
      }
      ivb.insert({it->second, v});
      return new_n;
    }
    bool hasKey(Vertex* v) const { return ivb.right.find(v) != ivb.right.end(); }
    bool hasKey(Name const& n) const { return ni.find(n) != ni.end(); }
    Vertex* at(Index const& i) const { return ivb.left.at(i); }
    Index const& at(Vertex* v) const { return ivb.right.at(v); }

   private:
    BMap ivb;
    Map ni;
    Index cur = 0;
  } vertex_map_;

  struct ArrivalVariable
  {
    MPVariable* max;
    MPVariable* min;
  };

  struct WirePairHash 
  {
    size_t operator()(WirePair const& w) const { return (size_t)w.first; }
  };

  struct DelayMap
  {
    struct MinMaxDelay
    {
      Delay min;
      Delay max;
      void store(Delay d)
      {
        if (fuzzyLess(d, min)) min = d;
        if (fuzzyLess(max, d)) max = d;
      }
    };
    typedef std::unordered_map<WirePair, MinMaxDelay, WirePairHash> Map;
    Delay const& max(WirePair const& w) const {return m.at(w).max;}
    Delay const& min(WirePair const& w) const {return m.at(w).min;}
    void insert(Index const& p, Index const& s, Delay const& d) {insert(WirePair(p,s), d);}
    bool hasKey(WirePair const& w) const {return m.find(w) != m.end();}
    void clear() {m.clear();}
   private:
    void insert(WirePair const& w, Delay const& d)
    {
      auto it = m.find(w);
      if (it == m.end())
        it = m.insert({w, {LRL_H_INFINITY, LRL_H_ZERO}}).first;
      it->second.store(d);
    }
    Map m;
  } delays_;

  typedef std::unordered_map<WirePair, MPVariable*, WirePairHash> WireMPVarMap;
  typedef std::unordered_map<Index, ArrivalVariable> VertexMPVarMap;
  typedef sta::Map<Sdc*, std::shared_ptr<VertexMPVarMap>> MMVertexMPVarMap;
  typedef sta::Set<Name> NameSet;

  // methods
  void makeVar(const Index& vertex_index);
  void makeWire(const Index& pred, const Index& succ);
  void makeVarFanins(const VertexSet& ends, VertexIndexSet& pi_set, WirePairSet& w_set);
  void makeVarAllModesCore(NameSet const& vio_ends_name);
  void makeVarAllModes(bool multi_mode);
  void findHoldVioNameCore(NameSet& vio_ends_name);
  void findHoldVioNameMM(bool multi_mode, NameSet& vio_ends_name);

  void genPrimeInConstraints(const VertexIndexSet& pi_set);
  void genPrimeOutConstraints(const VertexSet& ends);
  void genWireConstraints(WirePair const& wire);
  void genAllWireConstraints(WirePairSet const& w_set);

  void getDelay(const Index& pred_index,
                const Index& succ_index,
                Delay& min_delay,
                Delay& max_delay);
  Delay getCriLongArrival(Vertex* end) const;

  Index const& getIndex(Vertex* v);
  void findAllIndex();

  bool hasHoldViolation(Vertex* vertex,
                        const NameSet& vio_ends_name) const;
  // should run findHoldVioNaemMM before
  void getHoldViolations(const NameSet& vio_ends_name,
                         VertexSet& hold_failures);

  // can not do lp for 10^-10, should zoom in
  Delay zoomIn(Delay ori) const { return ori * LRL_H_SCALING; }
  Delay zoomOut(Delay ori) const { return ori / LRL_H_SCALING; }

  // insert buffers
  Pin* findFanoutPin(Vertex* pred, Vertex* succ) const;
  Vertex* insertHoldDelay(Vertex* drvr,
                          int buffer_count,
                          PinSeq& load_pins,
                          bool loads_have_out_port,
                          LibertyCell* buffer_cell);
  void sortHoldBuffers();
  LibertyCell* findHoldBuffer(Delay min_limit, int& count, Delay& real_delay);
  bool insertBuffer(const Index& vertex_index,
                    Delay decrease,
                    WirePairSet& drvr_pair,
                    Vertex* drvr = nullptr);
  void getAllWirePairFrom(const Index& vertex_index,
                          WirePairSet& wire_pair_set);
  void getNonZeroPaddingPred(VertexIndexSet& vio_pred_set);

  // variables
  Index var_index_ = 0;
  Index ct_index_ = 0;
  Slack sdc_worst_slack_ = LRL_H_ZERO;

  MPSolver* solver_;
  MMVertexMPVarMap arrival_;      // A/a
  WireMPVarMap wire_;             // w

  sta::Vector<BufferDelay> hold_buffers_;
  VertexIndexSet complete_vio_ends_;

  Options* options_;
};

class Laurel : public LaurelAdapter
{
 public:
  Laurel() : LaurelAdapter() { options_ = std::make_shared<Options>(); }
  void deleteLaurel();

  void initLaurel(OpenRoad* openroad,
                  Tcl_Interp* interp,
                  Logger* logger,
                  Gui* gui,
                  dbDatabase* db,
                  dbSta* sta,
                  SteinerTreeBuilder* stt_builder,
                  GlobalRouter* global_router);

  int fixHold(bool multi_mode);
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
  bool fixHoldCore(bool multi_mode);

  bool resizeAllCells(SetupFixer* fixer);

  bool checkVertexBudgeted(Vertex* pred, Vertex* succ);
  void showAllWireBud(WireBudgeting* guider);  // debug
  void wireBudgetingCore();

  void initFastOpt(OpenRoad* openroad);
  // bool quickFixSetup(Vertex* end);
  bool quickFixSetup(Vertex* v);
  void splitFanoutLoads(PathRef *drvr_path);

  bool isWorstPathFromInPort(Vertex* vertex, MinMax const* min_max);

  // variable
  std::shared_ptr<Options> options_;

  WireBudgeting* wire_bud_ = nullptr;
  ClockConcurrentFixer* ccopt_fixer_ = nullptr;
};

}  // namespace lrl