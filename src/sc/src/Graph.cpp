#include "Graph.h"

namespace sc {

void Graph::addEdge(const Graph::Edge &edge) {
  edges_.push_back(edge);
  vertexEdges_[edge.first].push_back(edge);
  vertexEdges_[edge.second].push_back(edge);
}

void Graph::addEdges(const std::vector<Edge> &edges) {
  for (const Edge &edge : edges) {
    addEdge(edge);
  }
}

std::vector<Graph::NodeId> Graph::dfs(const int start_node) {
  std::vector<bool> visited(getNumVertices());
  std::vector<NodeId> orders;

  if (getNumVertices() > 0) {
    dfsInternal(start_node, visited, orders);
  }

  return orders;
}

void Graph::dfsInternal(const NodeId node_id, std::vector<bool> &visited, std::vector<NodeId> &orders) {
  visited[node_id] = true;
  orders.push_back(node_id);

  const std::vector<Edge> &edges = vertexEdges_[node_id];
  for (auto edge_iter = edges.begin(); edge_iter != edges.end(); ++edge_iter) {
    if (!visited[edge_iter->first]) {
      dfsInternal(edge_iter->first, visited, orders);
    }

    if (!visited[edge_iter->second]) {
      dfsInternal(edge_iter->second, visited, orders);
    }

  }
}

} // namespace sc