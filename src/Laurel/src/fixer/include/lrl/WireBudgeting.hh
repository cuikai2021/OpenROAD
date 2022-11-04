#pragma once

#include "lrl/SetupFixer.hh"

namespace lrl {

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
  WireSet& getAllWirePairs() { return opt_wires_; }

  bool hasWire(const Wire& wire);
  Vertex* getVertex(Pin* pin) const;

 protected:
  virtual void initWire(Vertex* pred, Vertex* succ, Edge* e) override;

  virtual Delay costSlope(Wire const& w) override;
  virtual Delay getLowBound(Wire const& w) override;
  virtual Delay getHighBound(Wire const& w) override;
  virtual void addConstraintEdge(Wire const& w) override;

  SteinerTree* getTree(Vertex* drvr);
  uint64 getTreeDist(Wire const& w);
  SteinerPt findNextPt(SteinerTree* tree,
                       const SteinerPt& drvr_pt,
                       const SteinerPt& last);
  Vertex* getFanoutVertex(Wire const& wire);
  Vertex* getFaninVertex(Wire const& wire) const;

  virtual Delay getCellDelay(Vertex* vertex);
  void buildSpefDelayMap(VertexDelayMap& spef_delay_map);
  bool makeDelaySpefCore(VertexDelayMap& map, double factor);

  void deleteAllTrees();

  // variable
  WireSet opt_wires_;

  typedef std::unordered_map<Wire, Vertex*, WireHash> FaninMap;
  FaninMap wire_fanout_;
  typedef std::unordered_map<Wire, uint64, WireHash> WireDistMap;
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

  virtual void addConstraintEdge(Wire const& w) override;

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
  virtual Delay costSlope(Wire const& w) override;
  virtual Delay getLowBound(Wire const& w) override;
  virtual Delay getHighBound(Wire const& w) override;

  Delay getDrvrSlope();

  Delay wire_slope_ = LRL_H_ZERO;
};

}  // namespace lrl