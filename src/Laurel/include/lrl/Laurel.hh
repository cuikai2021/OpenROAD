#pragma once

#include <ortools/graph/min_cost_flow.h>
#include <ortools/linear_solver/linear_solver.h>

#include <unordered_map>
#include <unordered_set>

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
  // definitions
  struct VertexPairHash
  {
    size_t operator()(VertexPair const& p) const noexcept
    {
      return (size_t) p.first;
    }
  };

  template <class Key, class T, class Hash = std::hash<Key>, class V = T>
  struct CompareMap
  {
    typedef std::unordered_map<Key, T, Hash> Map;
    void insert(Key const& k, V const& t)
    {
      map.insert({k, T()}).first->second.store(t);
    }
    Map map;
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

  void report() const;

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
  typedef std::deque<Constraint> CstSeq;

  typedef float Area;
  struct HighLowBound
  {
    struct Bound
    {
      Delay delay;
      Area area;
    };
    bool isInf() const
    {
      return (fuzzyInf(high.area) || fuzzyInf(low.area));
    }
    Bound high;
    Bound low;
  };
  typedef std::unordered_map<VertexPair, HighLowBound, VertexPairHash> BoundMap;
  BoundMap bound_;

  struct IndexVertexMap
  {
    typedef std::unordered_map<Vertex*, Index> Map;
    Index& operator[](Vertex* v)
    {
      auto it = map.find(v);
      if (it == map.end())
        it = map.insert({v, id_++}).first;
      return it->second;
    }
    size_t size() const {return id_;}
    Map map;
   private:
    Index id_ = NodeIndex::T_INDEX + 1;
  } index_;

  typedef float Resistance;
  typedef float Capacitance;

  struct DelayFactor
  {
    Delay delay = LRL_H_ZERO;
    Resistance prev_drive = LRL_H_ZERO;
    LibertyPort* in_port = nullptr;
    DcalcAnalysisPt* dcalc_ap = nullptr;
    void store(DelayFactor const& f)
    {
      if (fuzzyLess(delay, f.delay)) {
        delay = f.delay;
        prev_drive = f.prev_drive;
        in_port = f.in_port;
        dcalc_ap = f.dcalc_ap;
      }
    }
  };
  struct DelayMap : public CompareMap<VertexPair, DelayFactor, VertexPairHash>
  {
    Delay delay(VertexPair const& p) const
    {
      auto it = map.find(p);
      if (it == map.end())
        return LRL_H_ZERO;
      return it->second.delay;
    }
  } delays_;

  typedef WirePair IdEdge;
  typedef std::pair<IdEdge, Delay> IdEdgeWeight;
  typedef sta::Set<IdEdgeWeight> IdEdgeWeightSet;

  // methods
  virtual void buildIndexMap(const VertexSet& ends);
  void buildDelay(PathExpanded& expanded,
                  VertexPair const& edge,
                  int succ_id,
                  DcalcAnalysisPt* dcalc_ap);
  virtual void buildAllFaninEdge(VertexSet const& ends,
                                 VertexSet& pi,
                                 VertexPairSet& s);
  virtual void buildALLEdgePrimeOut(const VertexSet& ends);

  virtual Delay costSlope(VertexPair const& p);

  void addConstraintBase(Constraint&&);
  void inline addAllConstraintsEdge(VertexPairSet const s);
  void inline addAllConstraintsPrimeIn(VertexSet& pi);
  virtual void addConstraintEdge(VertexPair const& p);
  virtual void addConstraintPrimeIn(Vertex* pi);
  virtual void addConstraintPrimeOut(Vertex* po);

  void buildBellmanGraph(IdEdgeWeightSet& bellman_graph);
  void initArrivalMap();
  bool updateArrivalMap(const IdEdgeWeightSet& bellman_graph);

  Cost delay2Cost(Delay delay);
  Delay cost2Delay(Cost cost) const;
  bool isInf(Cost cost) const { return cost >= LRL_S_INT_INF; }

  HighLowBound& getBound(VertexPair const& p);
  virtual Delay getLowBound(VertexPair const& p);
  virtual Delay getHighBound(VertexPair const& p);

  // decide resize cells
  struct ResizeInfo
  {
    DelayFactor* info = nullptr;
    Delay new_delay = INF;
    void store(ResizeInfo const& i)
    {
      if (fuzzyLess(i.new_delay, new_delay)) 
      {
        info = i.info;
        new_delay = i.new_delay;
      }
    }
  };
  typedef CompareMap<Vertex*, ResizeInfo> ResizeMap;
  LibertyCell* findResizeCell(Vertex* v, ResizeInfo const& i);
  Delay getNewDelay(VertexPair const& p) const;
  void getResizeVertex(ResizeMap& resize_vertex);

  // variables
  int bellman_max_iter_num_ = 0;

  SimpleMinCostFlow solver_;
  CstSeq constraints_;
  
  std::deque<Arrival> fixed_arrival_;

  Options* options_;
};

class ClockConcurrentFixer : public SetupFixer
{
 public:
  ClockConcurrentFixer(OpenRoad* openroad, Options* options)
      : SetupFixer(openroad, options){};

  virtual void initAlgorithm() override;
  Delay getUsefulSkew(Vertex const* vertex);

  void report_skews();

 protected:
  virtual void addConstraintPrimeOut(Vertex* po) override;
  virtual void addConstraintPrimeIn(Vertex* pi) override;
  void addConstraintPrimeInOut(Vertex* pi, Vertex* po);

  struct Reg2RegSkewMap
  {
    typedef std::unordered_map<Vertex*, Delay> NodeSkewMap;
    typedef std::unordered_map<Vertex*, std::shared_ptr<NodeSkewMap>> Map;
    void insert(Vertex* i, Vertex* o, Delay const& d)
    {
      insert_map(i2o, i, o, d);
      insert_map(o2i, o, i, d);
    }
    NodeSkewMap* find(Vertex* i, bool& is_i2o) const
    {
      is_i2o = true;
      auto it = i2o.find(i);
      if (it == i2o.end()) {
        is_i2o = false;
        it = o2i.find(i);
        if (it == o2i.end())
          return NULL;
      }
      return it->second.get();
    }

   private:
    void insert_map(Map& m, Vertex* k0, Vertex* k1, Delay const& d)
    {
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
  virtual ~WireBudgeting() { deleteAllTrees(); }

  virtual void initAlgorithm();

  bool makeDelaySpef() { return makeDelaySpefCore(spef_delay_, 1.0); }
  bool makeDelaySpefFct() { return makeDelaySpefCore(spef_delay_factor_, 2.0); }

  virtual void getNewWireLength(Vertex* pred,
                                Vertex* succ,
                                uint64& max_length,
                                Delay& bud_rate);
  VertexPairSet& getAllWirePairs() { return opt_wires_; }

  bool hasWire(const VertexPair& wire);
  Vertex* getVertex(Pin* pin) const;

 protected:
  virtual void buildAllFaninEdge(VertexSet const& ends,
                                 VertexSet& pi,
                                 VertexPairSet& s) override;

  virtual Delay costSlope(VertexPair const& p) override;
  virtual Delay getLowBound(VertexPair const& p) override;
  virtual Delay getHighBound(VertexPair const& p) override;
  virtual void addConstraintEdge(VertexPair const& p) override;

  SteinerTree* getTree(Vertex* drvr);
  uint64 getTreeDist(VertexPair const& p);
  SteinerPt findNextPt(SteinerTree* tree,
                       const SteinerPt& drvr_pt,
                       const SteinerPt& last);
  Vertex* getFanoutVertex(VertexPair const& wire);
  Vertex* getFaninVertex(VertexPair const& wire) const;

  virtual Delay getCellDelay(Vertex* vertex);
  void buildSpefDelayMap(VertexDelayMap& spef_delay_map);
  bool makeDelaySpefCore(VertexDelayMap& map, double factor);

  void deleteAllTrees();

  // variable
  VertexPairSet opt_wires_;

  typedef std::unordered_map<VertexPair, Vertex*, VertexPairHash> FaninMap;
  FaninMap wire_fanout_;
  typedef std::unordered_map<VertexPair, uint64, VertexPairHash> WireDistMap;
  WireDistMap real_wire_distance_;

  struct DrvrInfo
  {
    Vertex* vertex;
    SteinerTree* tree;
  };
  typedef std::unordered_map<Pin*, DrvrInfo> DrvrCacheMap;
  DrvrCacheMap drvr_cache_;

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

  virtual void addConstraintEdge(VertexPair const& p) override;

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
  virtual Delay costSlope(VertexPair const& p) override;
  virtual Delay getLowBound(VertexPair const& p) override;
  virtual Delay getHighBound(VertexPair const& p) override;

  Delay getDrvrSlope();

  Delay wire_slope_ = LRL_H_ZERO;
};

class HoldFixerTaskManager : public LaurelAdapter
{
 public:
  HoldFixerTaskManager(OpenRoad* openroad, Options* options);

  void findTasks(bool multi_mode);
  VertexSet* taskSet(size_t i) const { return tasks_.seq.at(i).get(); }
  size_t taskSize() const { return tasks_.seq.size(); }

 protected:
  void findTaskCore();
  void findTaskCoreMM();

  typedef std::unordered_set<Vertex*> VSet;
  void getAllRelatedFailures(Vertex* v,
                             VSet const& failures,
                             VertexSet& results);
  void findTaskBfs(VSet& failures);

  struct ExclusiveSet
  {
    void expand(VertexSet const& related, std::vector<Vertex*>& queue);
    typedef std::vector<std::shared_ptr<VertexSet>> SetSeq;
    SetSeq seq;
  } tasks_;
  Options* options_;
};

class HoldFixer : public LaurelAdapter
{
 public:
  HoldFixer(OpenRoad* openroad, Options* options);

  HoldResult resolve();

  bool insertAllBuffer();
  int getInsertedBufferCnt() const { return inserted_buffer_count_; }

  void initAlgorithm(bool multi_mode, VertexSet* v = nullptr);

  // backup sort buffer results
  struct BufferDelay
  {
    LibertyCell* buffer;
    Delay hold_delay;
    bool operator<(BufferDelay const& other) const
    {
      return this->hold_delay > other.hold_delay;
    }
  };
  typedef sta::Vector<BufferDelay> BufferSeq;
  BufferSeq* hold_buffers_ = nullptr;

 protected:
  // definitions
  enum ArrivalType
  {
    DEFAULT = 0,  // path start from 0
    HALF_AHEAD,   // path start from T/2
    SIZE          // total number of types
  };

  struct ArrivalVariableUnit
  {
    MPVariable* max;
    MPVariable* min;
  };
  template <class Key, class T, class Hash = std::hash<Key>>
  struct SharedPtrMap
  {
    typedef std::unordered_map<Key, std::shared_ptr<T>, Hash> Map;
    T& operator[](Key const& k)
    {
      auto it = map.find(k);
      if (it == map.end())
        it = map.insert({k, std::make_shared<T>()}).first;
      return *(it->second);
    }
    Map map;
  };
  typedef std::unordered_map<ArrivalType, ArrivalVariableUnit> ArrivalVariable;
  typedef SharedPtrMap<Vertex*, ArrivalVariable> VertexMPVarMap;
  typedef SharedPtrMap<Sdc*, VertexMPVarMap> MMVertexMPVarMap;

  typedef VertexPair Wire;
  typedef sta::Set<Wire> WireSet;
  typedef VertexPairHash WireHash;
  typedef SharedPtrMap<Wire, std::unordered_set<ArrivalType>, WireHash> WireTypeMap;
  struct WireInfo
  {
    MPVariable* var;
    Edge* edge;
  };
  typedef std::unordered_map<Wire, WireInfo, WireHash> WireMPVarMap;

  struct MinMaxDelay
  {
    Delay min = LRL_H_INFINITY;
    Delay max = LRL_H_ZERO;
    void store(Delay const& d)
    {
      if (fuzzyLess(d, min))
        min = d;
      if (fuzzyLess(max, d))
        max = d;
    }
  };
  typedef CompareMap<Wire, MinMaxDelay, WireHash, Delay> DelayMap;
  DelayMap delays_;

  typedef std::unordered_set<DcalcAnalysisPt*> APSet;
  APSet ap_;

  struct DrvrBfsBkwdIterator;

  // methods
  void initWire(Vertex* pred,
                Vertex* succ,
                Edge* e,
                WireTypeMap& w);
  void generateAllWire(Vertex* v,
                       DrvrBfsBkwdIterator* it,
                       WireTypeMap& w);

  void makeVar(Vertex* v, ArrivalType const& t = ArrivalType::DEFAULT);
  void registerWire(Wire const& w, Edge* e);
  void makeVarFanins(VertexSet const& ends, VertexSet& pi_set, WireTypeMap& w);
  void makeVarAllModesCore(VertexSet const& hold_failures_end);
  void findHoldVioCore(VertexSet& vio_ends);
  void findHoldVioMM(bool multi_mode, VertexSet& vio_ends);
  void buildFailureEnds(bool is_partial,
                        VertexSet* v,
                        VertexSet const& vio_ends);

  void genPrimeInConstraints(VertexSet const& pi_set);
  void genPrimeOutConstraints(const VertexSet& ends);
  void genWireConstraints(Wire const& wire, ArrivalType const& t);
  void genAllWireConstraints(WireTypeMap const& w);

  void getDelay(Wire const& w, Delay& min_delay, Delay& max_delay);
  Delay getCriLongArrival(Vertex* end) const;

  bool hasHoldViolation(Vertex* v, VertexSet const& vio_ends) const;
  void getHoldViolations(VertexSet const& vio_ends, VertexSet& hold_failures);

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
  bool insertBuffer(Vertex* insert_from,
                    Delay decrease,
                    WireSet& drvr_pair_set,
                    Vertex* drvr = nullptr);
  void getAllWireFrom(Vertex* v, WireSet& wire_set);
  void getNonZeroPaddingPred(VertexSet& vio_pred_set);
  Delay getWirePadding(Wire const& w) const;

  // variables
  Slack sdc_worst_slack_ = LRL_H_ZERO;

  std::unique_ptr<MPSolver> solver_;
  MMVertexMPVarMap arrival_;  // A/a
  WireMPVarMap wire_;         // w

  VertexSet complete_vio_ends_;

  Options* options_;
};

class Laurel : public LaurelAdapter
{
 public:
  Laurel() : LaurelAdapter()
  {
    options_ = std::make_shared<Options>();
    hold_buffers_ = std::make_shared<HoldFixer::BufferSeq>();
  }
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
  bool fixHoldCore(bool multi_mode, VertexSet* v = nullptr);

  bool resizeAllCells(SetupFixer* fixer);

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
  std::shared_ptr<HoldFixer::BufferSeq> hold_buffers_;

  WireBudgeting* wire_bud_ = nullptr;
  ClockConcurrentFixer* ccopt_fixer_ = nullptr;
};

}  // namespace lrl