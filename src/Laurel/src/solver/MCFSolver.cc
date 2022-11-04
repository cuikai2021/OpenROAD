#include "lrl/MCFSolver.hh"

namespace lrl {

////////////////////////////////////////////////////////
//
//  class MCFSolver
//
////////////////////////////////////////////////////////

using namespace operations_research;

MCFResult MCFSolver::resolve()
{
  // Add each arc.
  for (auto& c : constraints_)
    solver_.AddArcWithCapacityAndUnitCost(
        c.start_node, c.end_node, c.capacity, c.unit_cost);

  // Add node supplies.
  solver_.SetNodeSupply(NodeIndex::s_index, inf);
  solver_.SetNodeSupply(NodeIndex::t_index, -inf);
  return solver_.SolveMaxFlowWithMinCost();
  // return solver_.Solve();
};

void MCFSolver::forEachArc(
    std::function<void(Constraint const&, Cost)> const& f) const
{
  for (std::size_t i = 0; i < solver_.NumArcs(); ++i) {
    Constraint c = {solver_.Tail(i),
                    solver_.Head(i),
                    solver_.Capacity(i),
                    solver_.UnitCost(i)};
    f(c, solver_.Flow(i));
  }
}

void MCFSolver::addConstraint(Constraint&& c)
{
  constraints_.emplace_back(c);
}

void MCFSolver::addConstraintSrc(Index i, Cost c)
{
  addConstraint({NodeIndex::s_index, i, inf, c});
}
void MCFSolver::addConstraintSnk(Index i, Cost c)
{
  addConstraint({i, NodeIndex::t_index, inf, c});
}

}  // namespace lrl
