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
#define LP_PRIORITY 0.0000001
#define LP_STA_SCALING 1000000000
#define LP_ROUNDING_LIMIT 0.009f / LP_STA_SCALING

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
  float getWirePadding(WirePair wire_pair);
  WirePairSet getAllWirePairFrom(Vertex* vertex);
  VertexSet getNonZeroPaddingPred();

 protected:
  void makeVar(Vertex* vertex, MMVertexMPVarMap& arrival);
  void makeWire(Vertex* pred, Vertex* succ);
  void makeVarFanins(VertexSet fanins);
  void makeVarAllModes();

  int addConstraints(float low_bound, float high_bound);
  void genPIConstraints();
  void genPOConstraints(VertexSet* ends);
  void genWireConstraints(WirePair wire);

  float getRequired(Vertex* vertex, const MinMax* min_max);
  float getArrivalMax(Vertex* vertex);
  void makeDelay(Vertex* vertex);
  float getDelay(Vertex* vertex);
  float getCriLongArrival(Vertex* end);

  // can not do lp for 10^-10, should zoom in
  float zoomIn(float ori) { return ori * LP_SCALING; }
  float zoomOut(float ori) { return ori / LP_SCALING; }

  bool isPI(Vertex* vertex);

  // variables
  int ct_index_ = 0;
  int var_index_ = 0;

  MPSolver* solver_;
  MMVertexMPVarMap arrival_max_;  // A
  MMVertexMPVarMap arrival_min_;  // a
  WireMPVarMap wire_;             // w
  MPConstraintMap constraint_;

  sta::Map<Vertex*, float> delay_map_;
};

class LPResizer : public Resizer
{
 public:
  LPResizer() : Resizer(){};
  void fixHold();

 protected:
  bool fixHoldCore();
  bool insertBuffer(LPHoldFixer* fixer, Vertex* vertex);
  bool insertAllBuffer(LPHoldFixer* fixer);
  Pin* findFanoutPin(WirePair wire_pair);
  void insertHoldDelay(Vertex* drvr,
                       int buffer_count,
                       PinSeq& load_pins,
                       bool loads_have_out_port,
                       LibertyCell* buffer_cell);

  LibertyCell* findHoldBuffer(float min_limit);

};

}  // namespace rsz