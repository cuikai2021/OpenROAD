#include "lrl/LPSolver.hh"

namespace lrl {

////////////////////////////////////////////////////////
//
//  class LPSolver
//
////////////////////////////////////////////////////////

using namespace operations_research;

typedef LPSolver::Constraint Constraint;
typedef LPSolver::Unit Unit;
typedef LPSolver::LPResult LPResult;
typedef LPSolver::UnitCoeff UnitCoeff;
typedef LPSolver::Value Value;

Constraint* LPSolver::setEqual(Unit* u, Value v)
{
  Constraint* c = solver_->MakeRowConstraint(v, v, "");
  c->SetCoefficient(u, 1);
  return c;
}

Constraint* LPSolver::setCoeffGreater(Unit* u, Value coeff, Value lower)
{
  Constraint* c = solver_->MakeRowConstraint(lower, INF, "");
  c->SetCoefficient(u, coeff);
  return c;
}

Constraint* LPSolver::setPolynomialGreater(UnitCoeff* variables,
                                           size_t len,
                                           Value lower)
{
  Constraint* c = solver_->MakeRowConstraint(lower, INF, "");
  for (size_t i = 0; i < len; ++i) {
    auto& uc = variables[i];
    c->SetCoefficient(uc.unit, uc.coeff);
  }
  return c;
}

LPResult LPSolver::solve()
{
  MPObjective* const objective = solver_->MutableObjective();
  objective->SetMinimization();
  return solver_->Solve();
}


}  // namespace lrl
