#pragma once

namespace sc {
class Graph;

class ConstructStrategy {
 public:
  virtual void construct(Graph *const &graph) const = 0;
};
} // namespace sc

