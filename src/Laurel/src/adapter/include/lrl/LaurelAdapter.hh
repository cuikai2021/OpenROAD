#pragma once

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

using namespace sta;
using namespace rsz;

using rsz::Resizer;

typedef std::string Name;
typedef std::pair<Vertex*, Vertex*> Wire;
typedef sta::Set<Wire> WireSet;

class LaurelAdapter : public Resizer
{
 public:
  typedef sta::Map<Vertex*, Delay> VertexDelayMap;

  // adapter api for STA
  struct ArcDelayResult
  {
    ArcDelay delay;
    Slew slew;
  };
  ArcDelayResult predictGateDelay(Edge* e,
                                  TimingArc* arc,
                                  LibertyCell* cell,
                                  DcalcAnalysisPt const* dcalc_ap);

  ArcDelay findArcDelay(Edge* e,
                        TimingArc* arc,
                        DcalcAnalysisPt const* dcalc_ap);

 protected:
  // definitions
  struct WireHash
  {
    size_t operator()(Wire const& w) const noexcept { return (size_t) w.first; }
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
};

// add init constructor
// add shared option env
class LaurelBase : public LaurelAdapter
{
 public:
  LaurelBase(OpenRoad* openroad, Options* options);

 protected:
  Options* options_;
};

// `LaurelCollector` is an abstract base class
// defined how Laurel information collector iterate graph
// default to use BFS to iterate ends
class LaurelCollector : public LaurelBase
{
 public:
  LaurelCollector(OpenRoad* openroad, Options* options)
      : LaurelBase(openroad, options){};

 protected:
  friend struct DrvrBfsBkwdIterator;

  virtual void initWire(Vertex* pred, Vertex* succ, Edge* e) = 0;
  void iterateWiresBfs(VertexSet const& ends, VertexSet& pi);
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

}  // namespace lrl