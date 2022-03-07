#pragma once

#include <ortools/linear_solver/linear_solver.h>

#include "LPOptions.hh"
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

#define LP_SOLVER "SCIP"
#define LP_INFINITY (std::numeric_limits<float>::max)()
#define LP_ZERO 0.0f
#define LP_SCALING 100000
#define LP_PRIORITY 10000000
#define LP_STA_SCALING 1000000000
#define LP_ROUNDING_LIMIT 0.005f / LP_STA_SCALING
#define LP_BUFFER_SPILIT 2
#define LP_HIER_CHAR "/"

namespace rsz {

using namespace operations_research;
using namespace sta;

using operations_research::MPSolver;

typedef std::pair<std::string, std::string> WirePair;
typedef sta::Set<WirePair> WirePairSet;
typedef sta::Map<std::string, MPVariable*> VertexMPVarMap;
typedef sta::Map<Sdc*, VertexMPVarMap*> MMVertexMPVarMap;
typedef sta::Map<WirePair, MPVariable*> WireMPVarMap;
typedef sta::Map<int, MPConstraint*> MPConstraintMap;
typedef sta::Vector<std::string> VertexNameSeq;
typedef sta::Set<std::string> VertexNameSet;

typedef MPSolver::ResultStatus LPResult;

class LPHoldFixer : public Resizer
{
 public:
  LPHoldFixer(OpenRoad* openroad, LPOptions* options);
  virtual ~LPHoldFixer();

  LPResult doLP();
  float getWirePadding(WirePair wire_pair) const;
  WirePairSet getAllWirePairFrom(std::string vertex_name) const;
  VertexNameSet getNonZeroPaddingPred() const;

  Vertex* findRealVertex(std::string vertex_name) const;

 protected:
  void makeVar(std::string vertex_name, MMVertexMPVarMap& arrival);
  void makeWire(std::string pred, std::string succ);
  void makeVarFanins(VertexSeq fanins);
  void makeVarAllModes();
  void makeDelay(Vertex* vertex);
  void findHoldVioNameMM();

  int addConstraints(float low_bound, float high_bound);
  void genPIConstraints();
  void genPOConstraints(VertexSet* ends);
  void genWireConstraints(WirePair wire);

  float getRequired(Vertex* vertex, const MinMax* min_max) const;
  float getArrivalMax(Vertex* vertex) const;
  float getDelay(std::string vertex_name) const;
  float getCriLongArrival(Vertex* end) const;

  bool hasHoldViolation(Vertex* vertex) const;
  VertexSet getHoldViolations() const;  // should run findHoldVioNaemMM before

  VertexNameSeq getSortedNameSeq(VertexMPVarMap* mp_var_map);

  // can not do lp for 10^-10, should zoom in
  float zoomIn(float ori) const { return ori * LP_SCALING; }
  float zoomOut(float ori) const { return ori / LP_SCALING; }

  bool isPI(std::string vertex_name) const;

  // variables
  int ct_index_ = 0;
  int var_index_ = 0;

  MPSolver* solver_;
  MMVertexMPVarMap arrival_max_;  // A
  MMVertexMPVarMap arrival_min_;  // a
  WireMPVarMap wire_;             // w
  MPConstraintMap constraint_;

  sta::Map<std::string, float> delay_map_;
  VertexNameSet vio_ends_name_;

  LPOptions* options_;
};

class LPResizer : public Resizer
{
 public:
  LPResizer() : Resizer() { options_ = new LPOptions(); }
  virtual ~LPResizer() { delete options_; }
  void fixHold();

  LPOptions* getOptions() { return options_; }

 protected:
  bool fixHoldCore();

  int findInsertBufferCell(float max_padding, LibertyCell* buffer_cell);
  bool insertBuffer(LPHoldFixer* fixer, std::string vertex_name);
  bool insertAllBuffer(LPHoldFixer* fixer);
  Pin* findFanoutPin(LPHoldFixer* fixer, WirePair wire_pair) const;
  void insertHoldDelay(Vertex* drvr,
                       int buffer_count,
                       PinSeq& load_pins,
                       bool loads_have_out_port,
                       LibertyCell* buffer_cell);

  LibertyCell* findHoldBuffer(float min_limit);

  // variable
  LPOptions* options_;
};

}  // namespace rsz