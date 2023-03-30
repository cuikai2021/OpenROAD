#include "ChristofidesSpanningGraph.h"

namespace sc {

int ChristofidesTsp::run()
{
  _path.clear();

  if (_gr.maxNodeId() == -1)
    return _sum = 0;
  else if (_gr.maxNodeId() == 0) {
    _path.push_back(_gr.nodeFromId(0));
    return _sum = 0;
  } else if (_gr.maxNodeId() == 2) {
    _path.push_back(_gr.nodeFromId(0));
    _path.push_back(_gr.nodeFromId(1));
    return _sum = 2 * _cost[_gr.edgeFromId(0)];
  }

  // Compute min. cost spanning tree
  std::vector<lemon::ListGraph::Edge> tree;
  lemon::kruskal(_gr, _cost, std::back_inserter(tree));

  lemon::ListGraph::NodeMap<int> deg(_gr, 0);
  for (int i = 0; i != int(tree.size()); ++i) {
    lemon::ListGraph::Edge e = tree[i];
    ++deg[_gr.u(e)];
    ++deg[_gr.v(e)];
  }

  // Copy the induced subgraph of odd nodes
  std::vector<lemon::ListGraph::Node> odd_nodes;
  std::vector<sc::Point> points;
  std::unordered_map<int, int> id_map;
  int idx = 0;
  for (lemon::ListGraph::NodeIt u(_gr); u != lemon::INVALID; ++u) {
    if (deg[u] % 2 == 1) {
      odd_nodes.push_back(u);
      points.push_back(_points[_gr.id(u)]);
      id_map[idx++] = _gr.id(u);
    }
  }

  sc::Graph graph = sc::Graph(points);
  graph.construct(new sc::ZhouSpanningGraphStrategy);
  lemon::SmartGraph sgr;
  lemon::SmartGraph::EdgeMap<int> scost(sgr);
  for (int i = 0; i != int(odd_nodes.size()); ++i) {
    sgr.addNode();
  }
  for (size_t i = 0; i != graph.edges_.size(); ++i) {
    sc::Graph::NodeId l = graph.edges_[i].first;
    sc::Graph::NodeId r = graph.edges_[i].second;
    lemon::SmartGraph::Edge e
        = sgr.addEdge(sgr.nodeFromId(l), sgr.nodeFromId(r));
    int dis = (int) graph.vertices_[l].distance(graph.vertices_[r]);
    scost[e] = -dis;
  }

  // Compute min. cost perfect matching
  lemon::MaxWeightedPerfectMatching<lemon::SmartGraph,
                                    lemon::SmartGraph::EdgeMap<int>>
      mwpm(sgr, scost);
  bool flag = mwpm.run();
  if (!flag) {
    return -1;  // don't find a perfect matching
  }

  // Join the spanning tree and the matching
  lemon::SmartGraph sgr2;
  for (int i = 0; i <= _gr.maxNodeId(); ++i) {
    sgr2.addNode();
  }
  for (int i = 0; i != int(tree.size()); ++i) {
    int ui = _gr.id(_gr.u(tree[i])), vi = _gr.id(_gr.v(tree[i]));
    sgr2.addEdge(sgr2.nodeFromId(ui), sgr2.nodeFromId(vi));
  }
  for (lemon::SmartGraph::EdgeIt e(sgr); e != lemon::INVALID; ++e) {
    if (mwpm.matching(e)) {
      int i = sgr.id(sgr.u(e)), j = sgr.id(sgr.v(e));
      sgr2.addEdge(sgr2.nodeFromId(id_map[i]), sgr2.nodeFromId(id_map[j]));
    }
  }

  // Compute the tour from the Euler traversal
  lemon::SmartGraph::NodeMap<bool> visited(sgr2, false);
  for (lemon::EulerIt<lemon::SmartGraph> e(sgr2); e != lemon::INVALID; ++e) {
    lemon::SmartGraph::Node n = sgr2.target(e);
    if (!visited[n]) {
      _path.push_back(_gr.nodeFromId(sgr2.id(n)));
      visited[n] = true;
    }
  }

  _sum = 0;
  for (size_t i = 0; i < _path.size(); ++i) {
    int j = (i + 1) % _path.size();
    lemon::ListGraph::Node l = _path[i], r = _path[j];
    int id_l = _gr.id(l), id_r = _gr.id(r);
    sc::Point pi = _points[id_l], pj = _points[id_r];
    _sum += pi.distance(pj);
  }

  return _sum;
}

}  // namespace sc
