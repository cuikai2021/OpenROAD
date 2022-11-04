#pragma once

#include <functional>

#include "sta/Bfs.hh"

namespace sta {

class SearchPred;
class Edge;
class Vertex;
class Search;

}  // namespace lrl

namespace lrl {

using namespace sta;

class LaurelCollector;

// The template class T should be an interface
// Which must have method `initWire(Vertex*, Vertex*, Edge*)`
// Check more details of interface definition of `LaurelCollector`
struct DrvrBfsBkwdIterator : public BfsBkwdIterator
{
  using BfsIterator::enqueueAdjacentVertices;

  DrvrBfsBkwdIterator(BfsIndex i, SearchPred* p, LaurelCollector* s)
      : BfsBkwdIterator(i, p, (StaState*)s), fixer(s) {}

  virtual void enqueueAdjacentVertices(Vertex* vertex,
                                       SearchPred* search_pred,
                                       Level to_level) override
  {
    iterVertexEdge(vertex, [&](Vertex* mid, Edge* mid_edge) {
      if (ableToEnqueue(search_pred, mid, to_level, mid_edge))
        pushInQueue(mid, vertex, mid_edge);
      else
        iterVertexEdge(mid, [&](Vertex* pred, Edge* pred_edge) {
          if (ableToEnqueue(search_pred, pred, to_level, pred_edge))
            pushInQueue(pred, vertex, mid_edge);
        });
    });
  }
  LaurelCollector* fixer;

 protected:
  void iterVertexEdge(Vertex* v, std::function<void(Vertex*, Edge*)> const& f);
  bool ableToEnqueue(SearchPred* p, Vertex* v, Level const& l, Edge* e);
  void pushInQueue(Vertex* pred, Vertex* succ, Edge* e);

 private:
  DISALLOW_COPY_AND_ASSIGN(DrvrBfsBkwdIterator);
};

}  // namespace lrl