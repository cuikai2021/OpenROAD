#pragma once

#include "ConstructStrategy.h"
#include "Graph.h"

namespace sc {
class MinimalSpanningTree : public ConstructStrategy {
 public:
  explicit MinimalSpanningTree(const std::vector<Graph::Edge> &constraints = std::vector<Graph::Edge>{}) : constraints_{constraints} {}
 protected:
  std::vector<Graph::Edge> constraints_;
};
} // namespace sc
