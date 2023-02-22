#pragma once

#include "MinimalSpanningTree.h"

namespace sc {
class Kruskal : public MinimalSpanningTree {
 public:
  explicit Kruskal(const std::vector<Graph::Edge> &constraints = std::vector<Graph::Edge>{}) : MinimalSpanningTree{constraints} {}
  void construct(Graph *const &graph) const override;
};
} // namespace sc
