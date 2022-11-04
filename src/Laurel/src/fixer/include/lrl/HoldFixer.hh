#pragma once

#include <unordered_set>

#include "lrl/LaurelAdapter.hh"
#include "lrl/LPSolver.hh"
#include "lrl/Buffering.hh"

namespace lrl {

class HoldFixerTaskManager : public LaurelBase
{
 public:
  HoldFixerTaskManager(OpenRoad* openroad, Options* options)
    : LaurelBase(openroad, options) {};
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
};

class HoldFixer : public LaurelCollector, public HoldBufferingSource
{
 public:
  HoldFixer(OpenRoad* openroad, Options* options, LPSolver* solver)
    : LaurelCollector(openroad, options), solver_(solver) {};

  void initAlgorithm(bool multi_mode, VertexSet* v = nullptr);

  virtual Delay getWirePadding(Wire const& w) const override;
  virtual void getAllWireFrom(Vertex* v, WireSet& wire_set) override;
  virtual void getNonZeroPaddingPred(VertexSet& vio_pred_set) override;

 protected:
  // definitions
  enum ArrivalType
  {
    DEFAULT = 0,  // path start from 0
    HALF_AHEAD,   // path start from T/2
    SIZE          // total number of types
  };

  typedef LPSolver::Unit SVar;
  struct ArrivalVariableUnit
  {
    SVar* max;
    SVar* min;
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
  typedef SharedPtrMap<Wire, std::unordered_set<ArrivalType>, WireHash> WireTypeMap;
  struct WireInfo
  {
    typedef std::unordered_set<ArrivalType> TypeSet;
    typedef std::shared_ptr<TypeSet> SetPtr;
    struct MinMaxDelay
    {
      Delay min = INF;
      Delay max = INF;
      bool isInf() const { return fuzzyInf(min) || fuzzyInf(max); }
      void store(Delay const& d)
      {
        if (fuzzyLess(d, min))
          min = d;
        if (fuzzyLess(max, d) || fuzzyInf(max))
          max = d;
      }
    } delay;
    SVar* var;
    Edge* edge;
    SetPtr types;
    WireInfo(SVar* v, Edge* e)
      : var(v), edge(e) { types = std::make_shared<TypeSet>(); }
  };
  struct WireMPVarMap : public std::unordered_map<Wire, WireInfo, WireHash>
  {
    void clear_types() { for (auto& w : *this) w.second.types->clear(); }
  } wire_; // w

  // methods
  virtual void initWire(Vertex* pred, Vertex* succ, Edge* e) override;

  void makeVar(Vertex* v, ArrivalType const& t = ArrivalType::DEFAULT);
  void registerWire(Wire const& w, Edge* e);
  void makeVarAllModesCore(VertexSet const& hold_failures_end);
  void findHoldVioCore(VertexSet& vio_ends);
  void findHoldVioMM(bool multi_mode, VertexSet& vio_ends);
  void buildFailureEnds(bool is_partial,
                        VertexSet* v,
                        VertexSet const& vio_ends);
  void genPrimeInConstraints(VertexSet const& pi_set);
  void genPrimeOutConstraints(const VertexSet& ends);
  void genWireConstraints(Wire const& wire, ArrivalType const& t);
  void genAllWireConstraints();
  void setAllPriority();

  void getDelay(Wire const& w, Delay& min, Delay& max);
  Delay getCriLongArrival(Vertex* end) const;

  bool hasHoldViolation(Vertex* v, VertexSet const& vio_ends) const;
  void getHoldViolations(VertexSet const& vio_ends, VertexSet& hold_failures);

  // can not do lp for 10^-10, should zoom in
  Delay zoomIn(Delay ori) const { return ori * LRL_H_SCALING; }
  Delay zoomOut(Delay ori) const { return ori / LRL_H_SCALING; }

  // variables
  LPSolver* solver_;
  MMVertexMPVarMap arrival_;  // A/a
};

}  // namespace lrl