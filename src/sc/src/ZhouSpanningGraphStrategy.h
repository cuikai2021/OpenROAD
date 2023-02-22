#pragma once

#include "ConstructStrategy.h"
#include "Graph.h"

namespace sc {
class ZhouSpanningGraphStrategy : public ConstructStrategy {
 public:
  void construct(Graph *const &graph) const override;
};
} // namespace sc
