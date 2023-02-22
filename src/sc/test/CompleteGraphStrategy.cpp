#include "CompleteGraphStrategy.h"
#include "Graph.h"

namespace sc {
void CompleteGraphStrategy::construct(Graph *const &graph) const {
  for (int i = 0; i < graph->getNumVertices(); i++) {
    for (int j = 0; j < i; j++) {
      graph->addEdge(std::pair(i, j));
    }
  }
}
} //namespace sc