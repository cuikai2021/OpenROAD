#pragma once

#include <ortools/linear_solver/linear_solver.h>

#include "Resizer.hh"
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

#define LP_INFINITY (std::numeric_limits<float>::max)()
#define LP_SCALING 100000
#define LP_PRIORITY 10000000
#define LP_STA_SCALING 1000000000
#define LP_ROUNDING_LIMIT 0.005f / LP_STA_SCALING
#define LP_BUFFER_SPILIT 2

namespace rsz {

using namespace operations_research;
using namespace sta;

using operations_research::MPSolver;

typedef std::pair<Vertex*, Vertex*> WirePair;
typedef sta::Set<WirePair> WirePairSet;
typedef sta::Map<Vertex*, MPVariable*> VertexMPVarMap;
typedef sta::Map<Sdc*, VertexMPVarMap*> MMVertexMPVarMap;
typedef sta::Map<WirePair, MPVariable*> WireMPVarMap;
typedef sta::Map<int, MPConstraint*> MPConstraintMap;

typedef MPSolver::ResultStatus LPResult;

class LPHoldFixer : public Resizer
{
 public:
  LPHoldFixer(OpenRoad* openroad);
  virtual ~LPHoldFixer();

  LPResult doLP();
  float getWirePadding(WirePair wire_pair) const;
  WirePairSet getAllWirePairFrom(Vertex* vertex) const;
  VertexSet getNonZeroPaddingPred() const;

  std::string getVertexName(Vertex* vertex) const;
  Vertex* findRealVertex(Vertex* vertex) const;

 protected:
  void makeVar(Vertex* vertex, MMVertexMPVarMap& arrival);
  void makeWire(Vertex* pred, Vertex* succ);
  void makeVarFanins(VertexSeq fanins);
  void makeVarAllModes();
  void makeDelay(Vertex* vertex);
  void findHoldVioNameMM(float slack_margin);

  int addConstraints(float low_bound, float high_bound);
  void genPIConstraints();
  void genPOConstraints(VertexSet* ends);
  void genWireConstraints(WirePair wire);

  float getRequired(Vertex* vertex, const MinMax* min_max) const;
  float getArrivalMax(Vertex* vertex) const;
  float getDelay(Vertex* vertex) const;
  float getCriLongArrival(Vertex* end) const;

  bool hasHoldViolation(Vertex* vertex) const;
  VertexSet getHoldViolations() const;  // should run findHoldVioNaemMM before

  VertexSeq getSortedVertexSeq(VertexMPVarMap* mp_var_map);

  // can not do lp for 10^-10, should zoom in
  float zoomIn(float ori) const { return ori * LP_SCALING; }
  float zoomOut(float ori) const { return ori / LP_SCALING; }

  bool isPI(Vertex* vertex) const;

  // variables
  int ct_index_ = 0;
  int var_index_ = 0;

  MPSolver* solver_;
  MMVertexMPVarMap arrival_max_;  // A
  MMVertexMPVarMap arrival_min_;  // a
  WireMPVarMap wire_;             // w
  MPConstraintMap constraint_;

  sta::Map<Vertex*, std::pair<std::string, float>> delay_map_;
  sta::Set<std::string> vio_ends_name_;
};

class LPResizer : public Resizer
{
 public:
  LPResizer() : Resizer(){};
  void fixHold();

 protected:
  bool fixHoldCore();

  int findInsertBufferCell(float max_padding, LibertyCell* buffer_cell);
  bool insertBuffer(LPHoldFixer* fixer, Vertex* vertex);
  bool insertAllBuffer(LPHoldFixer* fixer);
  Pin* findFanoutPin(LPHoldFixer* fixer, WirePair wire_pair) const;
  void insertHoldDelay(Vertex* drvr,
                       int buffer_count,
                       PinSeq& load_pins,
                       bool loads_have_out_port,
                       LibertyCell* buffer_cell);

  LibertyCell* findHoldBuffer(float min_limit);

  // constant
  const float uncertainty_ = 0.1f;
  const int retry_limit_ = 2;
};

}  // namespace rsz