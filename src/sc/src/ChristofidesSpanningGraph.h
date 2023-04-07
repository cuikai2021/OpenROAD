#pragma once

#include <lemon/euler.h>
#include <lemon/full_graph.h>
#include <lemon/kruskal.h>
#include <lemon/list_graph.h>
#include <lemon/matching.h>
#include <lemon/smart_graph.h>

#include <unordered_map>

#include "Kruskal.h"
#include "ZhouSpanningGraphStrategy.h"

namespace sc {

class ChristofidesTsp
{
 private:
  const lemon::ListGraph& _gr;
  const lemon::ListGraph::EdgeMap<int>& _cost;
  std::vector<Graph::NodeId> _path;
  int _sum;
  std::vector<sc::Point> _points;

 public:
  ChristofidesTsp(const lemon::ListGraph& gr,
                  const lemon::ListGraph::EdgeMap<int>& cost)
      : _gr(gr), _cost(cost)
  {
  }

  void setPoints(const std::vector<sc::Point>& points) { _points = points; }
  int run();
  int tourCost() const { return _sum; }
  const std::vector<Graph::NodeId>& tourNodes() const { return _path; }
  int startId() const {return 0; }
  int stopId() const {return _points.size() - 1; }
  bool isEndPoint(const int node_id) {return (node_id == startId() || node_id == stopId()); }
  bool isStartStopEdge(const int id1, const int id2) {
    return (id1 == startId() && id2 == stopId()) || (id1 == stopId() && id2 == startId());
  }
};

}  // namespace sc
