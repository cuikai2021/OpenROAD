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
  std::vector<lemon::ListGraph::Node> _path;
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
  const std::vector<lemon::ListGraph::Node>& tourNodes() const { return _path; }
};

}  // namespace sc
