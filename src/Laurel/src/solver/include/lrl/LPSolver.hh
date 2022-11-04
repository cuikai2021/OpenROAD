#pragma once

#include <ortools/linear_solver/linear_solver.h>

namespace lrl {

using namespace operations_research;
using operations_research::MPSolver;


class LPSolver
{
 public:
  typedef float Value;
  typedef MPVariable Unit;
  typedef MPConstraint Constraint;
  typedef MPSolver::ResultStatus LPResult;

 private:  
  static constexpr Value INF = (std::numeric_limits<Value>::max)();
  static constexpr Value ZERO = 0.0f;
 
 public:
  LPSolver(const char* type): solver_(MPSolver::CreateSolver(type)) {}
  Unit* makeUnit() { return solver_->MakeNumVar(ZERO, INF, ""); }

  LPResult solve();
  Value solutionValue(Unit* u) const { return u->solution_value(); }
  Value objectValue() const { return solver_->MutableObjective()->Value(); }
  void setPriority(Unit* u, Value e) { solver_->MutableObjective()->SetCoefficient(u, e); }

  // add constraints
  Constraint* setEqual(Unit* u, Value v);
  Constraint* setGreater(Unit* u, Value lower) { return setCoeffGreater(u, 1, lower); }
  Constraint* setLess(Unit* u, Value upper) { return setCoeffGreater(u, -1, -upper); }
  Constraint* setCoeffGreater(Unit* u, Value coeff, Value lower);
  
  struct UnitCoeff
  {
    Unit* unit;
    Value coeff;
  };
  Constraint* setPolynomialGreater(/*array*/UnitCoeff* variables,
                                   size_t len,
                                   Value lower);

  
 protected:
  std::unique_ptr<MPSolver> solver_;
};

}  // namespace lrl
