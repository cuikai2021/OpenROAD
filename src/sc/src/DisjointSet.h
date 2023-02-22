#pragma once

#include <vector>

namespace sc {
class DisjointSet {
 public:
  /**
   * Create a disjoint set with n nodes
   */
  explicit DisjointSet(int n);
  /**
   * Queries the root of a node
   */
  int findRoot(int x);
  /**
   * Merge two nodes
   */
  void merge(int x, int y);
 private:
  std::vector<int> father;
  std::vector<int> rank;
};
} // namespace sc
