#include <assert.h>
#include "ChristofidesSpanningGraph.h"

namespace sc {

int ChristofidesTsp::run()
{
  _path.clear();

  if (_gr.maxNodeId() == -1)
    return _sum = 0;
  else if (_gr.maxNodeId() == 0) {
    _path.push_back(Graph::NodeId(0));
    return _sum = 0;
  } else if (_gr.maxNodeId() == 2) {
    _path.push_back(Graph::NodeId(0));
    _path.push_back(Graph::NodeId(1));
    return _sum = 2 * _cost[_gr.edgeFromId(0)];
  }

  // Compute min. cost spanning tree
  std::vector<lemon::ListGraph::Edge> tree;
  lemon::kruskal(_gr, _cost, std::back_inserter(tree));

  bool has_start_to_stop = false;
  lemon::ListGraph::NodeMap<int> deg(_gr, 0);
  for (int i = 0; i != int(tree.size()); ++i) {
    lemon::ListGraph::Edge e = tree[i];
    ++deg[_gr.u(e)];
    ++deg[_gr.v(e)];

    if (isStartStopEdge(_gr.id(_gr.u(e)), _gr.id(_gr.v(e)))) {
      has_start_to_stop = true;
    }
  }
  if (!has_start_to_stop) {// add a dummy edge from start to stop
    ++deg[_gr.nodeFromId(startId())];
    ++deg[_gr.nodeFromId(stopId())];
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
  if (!flag) { // don't find a perfect matching, use complete graph again
    sgr.clear();

    for (int i = 0; i != int(odd_nodes.size()); ++i) {
      sgr.addNode();
    }
    for (size_t i = 0; i < points.size(); i++) {
      for (size_t j = 0; j < i; j++) {
        lemon::SmartGraph::Edge e
            = sgr.addEdge(sgr.nodeFromId(i), sgr.nodeFromId(j));
        int dis = (int) points[i].distance(points[j]);
        scost[e] = -dis;
      }
    }

    mwpm.run();
  }

  // Join the spanning tree and the matching
  lemon::SmartGraph sgr2;
  for (int i = 0; i <= _gr.maxNodeId(); ++i) {
    sgr2.addNode();
  }
  for (int i = 0; i != int(tree.size()); ++i) {
    int ui = _gr.id(_gr.u(tree[i])), vi = _gr.id(_gr.v(tree[i]));
    if (!isStartStopEdge(ui, vi)) {
      sgr2.addEdge(sgr2.nodeFromId(ui), sgr2.nodeFromId(vi));
    }
  }
  for (lemon::SmartGraph::EdgeIt e(sgr); e != lemon::INVALID; ++e) {
    if (mwpm.matching(e)) {
      int i = id_map[sgr.id(sgr.u(e))], j = id_map[sgr.id(sgr.v(e))];
      if (!isStartStopEdge(i, j)) {
        sgr2.addEdge(sgr2.nodeFromId(i), sgr2.nodeFromId(j));
      }
    }
  }

  // Compute the tour from the Euler traversal
  lemon::SmartGraph::Node next_node;
  bool is_first_node = true;
  std::vector<bool> visited(_points.size());
  for (lemon::EulerIt<lemon::SmartGraph> e(sgr2); e != lemon::INVALID; ++e) {
    lemon::SmartGraph::Node start = sgr2.source(e);
    int id = sgr2.id(start);

    if (is_first_node) {
      is_first_node = false;
      assert(isEndPoint(id));
    } else if (isEndPoint(id)) {
      continue; // the end point must be the first/last point of the path
    }

    if (!visited[id]) {
      _path.push_back(Graph::NodeId(id));
      visited[id] = true;
    }

    next_node = sgr2.target(e);
  }
  assert(isEndPoint(sgr2.id(next_node)));
  _path.push_back(Graph::NodeId(sgr2.id(next_node)));

  if (_path[0] != startId()) { // make sure start id is at first on the path
    std::reverse(_path.begin(), _path.end());
  }

  _sum = 0;
  for (size_t i = 0; i < _path.size(); ++i) {
    int j = (i + 1) % _path.size();
    Graph::NodeId l = _path[i], r = _path[j];
    sc::Point pi = _points[l], pj = _points[r];
    _sum += pi.distance(pj);
  }

  return _sum;
}

}  // namespace sc
