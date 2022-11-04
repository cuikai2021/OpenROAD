#include "DrvrBfsBkwdIterator.hh"
#include "lrl/LaurelAdapter.hh"

namespace lrl {

using namespace sta;

void DrvrBfsBkwdIterator::pushInQueue(Vertex* pred, Vertex* succ, Edge* e)
{
  enqueue(pred);
  fixer->initWire(pred, succ, e);
}

bool DrvrBfsBkwdIterator::ableToEnqueue(SearchPred* p, Vertex* v, Level const& l, Edge* e)
{
  return v->level() >= l
          && p->searchFrom(v)
          && p->searchThru(e)
          && v->isDriver(network_)
          && !search_->isClock(v);
}

void DrvrBfsBkwdIterator::iterVertexEdge(Vertex* v, std::function<void(Vertex*, Edge*)> const& f)
{
  if (search_pred_->searchTo(v)) {
    VertexInEdgeIterator edge_iter(v, graph_);
    while (edge_iter.hasNext()) {
      Edge* e = edge_iter.next();
      Vertex* pv = e->from(graph_);
      f(pv, e);
    }
  }
}

}  // namespace lrl
