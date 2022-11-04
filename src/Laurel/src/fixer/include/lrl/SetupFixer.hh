#pragma once

#include "lrl/LaurelAdapter.hh"
#include "lrl/MCFSolver.hh"
#include "lrl/BellmanGraph.hh"
#include "lrl/Replacer.hh"

namespace lrl {

class SetupFixer : public LaurelCollector, public SetupReplacerSource
{
 public:
  SetupFixer(OpenRoad* openroad, Options* options)
    : LaurelCollector(openroad, options) { resizePreamble(); }
  virtual ~SetupFixer(){};

  virtual bool initAlgorithm(bool iterative);
  virtual void initAlgorithm() { initAlgorithm(false); }
  MCFResult resolve() { return solver_.resolve(); }
  bool remap();

  virtual void getResizeVertex(ResizeMap& resize_vertex);

  void report() const;

 protected:
  // definitions
  typedef float Area;
  struct WireInfo
  {
    struct HighLowBound
    {
      struct Bound
      {
        Delay delay = INF;
        Area area = INF;
      };
      Bound high;
      Bound low;
      bool isAreaInf() const { return fuzzyInf(high.area) || fuzzyInf(low.area); }
      bool isDelayInf() const { return fuzzyInf(high.area) || fuzzyInf(low.area); }
      HighLowBound& setDelay(Delay d)
      {
        high.delay = d;
        low.delay = d;
        return *this;
      }
    } bound;
    DelayFactor factor;
    void store(DelayFactor const& f)
    {
      if (fuzzyLess(factor.delay, f.delay))
        factor = f;
    }
  };
  typedef WireInfo::HighLowBound HighLowBound;
  struct WireMap : public CompareMap<Wire, WireInfo, WireHash, DelayFactor>
  {
    Delay delay(Wire const& w) const
    {
      auto it = map.find(w);
      if (it == map.end())
        return LRL_H_ZERO;
      return it->second.factor.delay;
    }
  } wire_;

  // methods
  virtual void initWire(Vertex* pred, Vertex* succ, Edge* e) override;

  virtual void buildIndexMap(const VertexSet& ends);
  void buildDelay(Wire const& w, Edge* e);

  HighLowBound& getBound(Wire const& w);
  virtual Delay getLowBound(Wire const& w);
  virtual Delay getHighBound(Wire const& w);
  virtual Delay costSlope(Wire const& w);

  void inline addAllConstraintsEdge();
  void inline addAllConstraintsPrimeIn(VertexSet const& pi);
  void inline addALLConstraintsPrimeOut(VertexSet const& ends);
  virtual void addConstraintEdge(Wire const& w);
  virtual void addConstraintPrimeIn(Vertex* pi);
  virtual void addConstraintPrimeOut(Vertex* po);

  void buildBellmanGraph();

  Cost delay2Cost(Delay delay);
  Delay cost2Delay(Cost cost) const;
  bool isInf(Cost cost) const { return cost >= MCFSolver::inf; }

  Delay getNewDelay(Wire const& w) const;

  // variables
  struct DelayLess
  {
    bool operator()(Delay const& l, Delay const& r) const
    { return fuzzyLess(l, r); }
  };
  BellmanGraph<Vertex*, Delay, DelayLess> index_graph_;
  MCFSolver solver_;

  int bellman_max_iter_num_ = 0;
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

}  // namespace lrl