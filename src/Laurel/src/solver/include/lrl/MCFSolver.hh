#pragma once

#include <ortools/graph/min_cost_flow.h>

#include <functional>
#include <set>

namespace lrl {

using namespace operations_research;

typedef int32_t MCFResult;
typedef int32_t Index;
typedef int64_t Cost;

enum NodeIndex
{
  s_index = 0,
  t_index
};

class MCFSolver
{
 public:

 protected:
  struct Constraint
  {
    Index start_node;
    Index end_node;
    Cost capacity;
    Cost unit_cost;
  };
  typedef std::deque<Constraint> CstSeq;
  CstSeq constraints_;
  SimpleMinCostFlow solver_;

 public:
  static constexpr int32_t inf = (std::numeric_limits<int32_t>::max)();
  static constexpr int32_t zero = 0;

  MCFResult resolve();
  Cost optimalCost() const { return solver_.OptimalCost(); }

  void forEachArc(std::function<void(Constraint const&, Cost)> const& f) const;

  void addConstraint(Constraint&&);
  void addConstraintSrc(Index i, Cost c);
  void addConstraintSnk(Index i, Cost c);
};

}  // namespace lrl
