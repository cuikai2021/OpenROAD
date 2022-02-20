#pragma once

#include "Resizer.hh"
#include "ortools/linear_solver/linear_solver.h"

#define MAX_BUF_LEN 256
#define INFINITY (std::numeric_limits<double>::max)()
#define ARR_MAX_NAME "A[%s]"
#define ARR_MIN_NAME "a[%s]"
#define ARR_WIRE_NAME "w[%s][%s]"

using namespace operations_research;

namespace rsz {

const double infinity = (std::numeric_limits<T>::max)();

typedef std::pair<Vertex*, Vertex*> WirePair;
typedef sta::Map<Vertex*, MPVariable*> VertexMPVarMap;
typedef sta::Map<Sdc*, VertexMPVarMap> MMVertexMPVarMap;
typedef sta::Map<WirePair, MPVariable*> WireMPVarMap;
typedef sta::Map<const char*, MPConstraint*> MPConstraintMap;

class LPResolver : public StaState
{
 public:
  LPResolver(Vertex* end)
      : end_(end), StaState(), solver_(MPSolver::CreateSolver("GLOP")){init()};
  void init();
  void doLP();
  double getWirePadding(Vertex* pred, Vertex* succ);

 protected:
  void makeVar(Sdc* sdc, Vertex* vertex);
  void genPIConstraints();
  void genPOConstraints();
  void genWireConstraints();
  const char[MAX_BUF_LEN] getVarName(Vertex* vertex, const char* fmt, ...);
  __attribute__((format(printf, 3, 4)));

  const char[MAX_BUF_LEN] getWireName(Vertex* pred,
                                      Vertex* succ,
                                      const char* fmt,
                                      ...);
  __attribute__((format(printf, 4, 5)));

  // variables
  int ct_index_ = 0;

  Vertex* end_;
  MPSolver solver_;
  MMVertexMPVarMap arrival_max_;  // A
  MMVertexMPVarMap arrival_min_;  // a
  WireMPVarMap wire_;             // w
  MPConstraintMap constraint_;

  sta::Map<Sdc*, WirePair> wire_list_;
  dbSta sta_;
}

class LPResizer : public Resizer
{
 public:
  LPResizer();
};

}  // namespace rsz