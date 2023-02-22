#include <algorithm>
#include "Kruskal.h"
#include "DisjointSet.h"

namespace sc {

void Kruskal::construct(Graph *const &graph) const {
  DisjointSet disjoint_set{graph->getNumVertices()};

  std::vector<std::pair<int, int>> edges = constraints_;

  // Sort the edges in ascending order of length(Manhattan Distance)
  auto compare = [&graph] (auto edge1, auto edge2) {
    auto pt1 = graph->getVertex(edge1.first);
    auto pt2 = graph->getVertex(edge1.second);
    double distance1 = std::abs(pt1.x - pt2.x) + std::abs(pt1.y - pt2.y);

    pt1 = graph->getVertex(edge2.first);
    pt2 = graph->getVertex(edge2.second);
    double distance2 = std::abs(pt1.x - pt2.x) + std::abs(pt1.y - pt2.y);

    return distance1 < distance2;
  };
  std::sort(edges.begin(), edges.end(), compare);

  for (auto [u, v] : edges) {
    if (disjoint_set.findRoot(u) != disjoint_set.findRoot(v)) {
      disjoint_set.merge(u, v);
      graph->addEdge(std::pair(u, v));
    }
  }
}

} // namespace sc