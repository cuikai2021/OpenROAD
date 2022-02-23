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

#define LP_MAX_BUF 256
#define LP_INFINITY (std::numeric_limits<float>::max)()
#define ARR_MAX_NAME "A[%s]"
#define ARR_MIN_NAME "a[%s]"
#define ARR_WIRE_NAME "w[%s][%s]"

namespace rsz {

using namespace operations_research;
using namespace sta;

using operations_research::MPSolver;

typedef std::pair<Vertex*, Vertex*> WirePair;
typedef sta::Set<WirePair> WirePairSet;
typedef sta::Map<Sdc*, WirePairSet*> WireListMap;
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
  void makeVar(Vertex* vertex, MMVertexMPVarMap &arrival);
  void makeWire(Vertex* pred, Vertex* succ);
  void makeVarFanins(VertexSet fanins);
  void makeVarAllModes();

  int addConstraints(float low_bound, float high_bound);
  void genPIConstraints();
  void genPOConstraints();
  void genWireConstraints();

  float getRequired(Vertex* vertex, const MinMax* min_max);
  void makeDelay(Vertex* vertex);
  float getDelay(Vertex* vertex);

  bool isPI(Vertex* vertex);

  // variables
  int ct_index_ = 0;
  int var_index_ = 0;
  int wire_index_ = 0;

  MPSolver* solver_;
  MMVertexMPVarMap arrival_max_;  // A
  MMVertexMPVarMap arrival_min_;  // a
  WireMPVarMap wire_;             // w
  MPConstraintMap constraint_;

  WireListMap wire_list_;
  sta::Map<Vertex *, float> delay_map_;
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

  LibertyCell* findHoldBuffer(float min_limit, float max_limit);

  const int max_insert_num_ = 10;
  float uncertainty_ = 0.001f;
};

}  // namespace rsz