#pragma once

#include <vector>
#include <iostream>
#include "ConstructStrategy.h"

namespace sc {

struct ConstructStrategy;

class Point {
 public:
  double x, y;
  Point() {};
  Point(double x_, double y_) : x(x_), y(y_) {}
  double distance(Point o) {
    return std::abs(x - o.x) + std::abs(y - o.y);
  }

  friend inline std::ostream& operator<<(std::ostream& os, const Point& gp) {
    os << "(" << gp.x << "," << gp.y << ")";
    return os;
  }
};

class Graph {
 public:
  typedef int NodeId;
  typedef std::pair<NodeId, NodeId> Edge;

  std::vector<Point> vertices_;
  std::vector<Edge> edges_;
  std::vector<std::vector<Edge>> vertexEdges_;

  Graph(const std::vector<Point> &vertices, const std::vector<Edge> &edges = std::vector<Edge>{})
      : vertices_{vertices}, edges_{edges} {
    vertexEdges_.resize(vertices_.size());
  }

  void construct(ConstructStrategy *strategy) {
    strategy->construct(this);
  }

  std::vector<NodeId> dfs(const int start_node);

  void addEdge(const Edge &edge);
  void addEdges(const std::vector<Edge> &edges);

  int getNumVertices() const { return vertices_.size(); }
  int getNumEdges() const { return edges_.size(); }

  Point getVertex(NodeId id) const { return vertices_[id]; }
  Edge getEdge(NodeId id) const { return edges_[id]; }

 private:
  void dfsInternal(const NodeId node_id, std::vector<bool> &visited, std::vector<NodeId> &orders);
};

} // namespace sc