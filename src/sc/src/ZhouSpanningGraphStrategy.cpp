#include <algorithm>
#include <map>
#include "ZhouSpanningGraphStrategy.h"
#include "Graph.h"

namespace sc {

class Node : public Point {
 public:
  Point* point;
  int id;
  Node(double x, double y, int id_) : Point(x, y), id(id_) {}
};

bool byYplusX(Node n1, Node n2) {
  return n1.y + n1.x <= n2.y + n2.x;
}

bool byYminusX(Node n1, Node n2) {
  return n1.y - n1.x <= n2.y - n2.x;
}

void ZhouSpanningGraphStrategy::construct(Graph *const &graph) const {
  std::vector<Node> nodes;
  for (int i = 0; i < graph->getNumVertices(); i ++) {
    auto vertex = graph->getVertex(i);
    nodes.push_back(Node(vertex.x, vertex.y, i));
  }

  std::sort(nodes.begin(), nodes.end(), byYplusX);
  auto comp_by_x = [](const Point& a, const Point& b) { return a.x >= b.x; };
  std::map<Point, int, decltype(comp_by_x)> comp_by_x_map(comp_by_x);
  auto comp_by_x_minus_y = [](const Point& a, const Point& b) { return a.x - a.y >= b.x - b.y; };
  std::map<Point, int, decltype(comp_by_x_minus_y)> comp_by_x_minus_y_map(comp_by_x_minus_y);

  for (size_t i = 0; i < nodes.size(); i++) {
    Node node = nodes[i];
    for (auto it_0 = comp_by_x_map.upper_bound(Point(node.x, node.y)); it_0 != comp_by_x_map.end(); ) {
      if (it_0->first.x - it_0->first.y <= node.x - node.y) {
        break;
      }
      graph->addEdge(std::pair(node.id, it_0->second));
      it_0 = comp_by_x_map.erase(it_0);
    }
    comp_by_x_map[Point(node.x, node.y)] = node.id;

    for (auto it_1 = comp_by_x_minus_y_map.upper_bound(Point(node.x, node.y)); it_1 != comp_by_x_minus_y_map.end(); ) {
      if (it_1->first.y >= node.y) {
        break;
      }
      graph->addEdge(std::pair(node.id, it_1->second));
      it_1 = comp_by_x_minus_y_map.erase(it_1);
    }
    comp_by_x_minus_y_map[Point(node.x, node.y)] = node.id;
  }

  std::sort(nodes.begin(), nodes.end(), byYminusX);
  auto comp_by_y = [](const Point& a, const Point& b) { return a.y >= b.y; };
  std::map<Point, int, decltype(comp_by_y)> comp_by_y_map(comp_by_y);
  auto comp_by_x_plus_y = [](const Point& a, const Point& b) { return a.x + a.y >= b.x + b.y; };
  std::map<Point, int, decltype(comp_by_x_plus_y)> comp_by_x_plus_y_map(comp_by_x_plus_y);

  for (size_t i = 0; i < nodes.size(); i++) {
    Node node = nodes[i];
    for (auto it_0 = comp_by_y_map.upper_bound(Point(node.x, node.y)); it_0 != comp_by_y_map.end(); ) {
      if (it_0->first.x + it_0->first.y <= node.x + node.y) {
        break;
      }
      graph->addEdge(std::pair(node.id, it_0->second));
      it_0 = comp_by_y_map.erase(it_0);
    }
    comp_by_y_map[Point(node.x, node.y)] = node.id;

    for (auto it_1 = comp_by_x_plus_y_map.upper_bound(Point(node.x, node.y)); it_1 != comp_by_x_plus_y_map.end(); ) {
      if (it_1->first.x <= node.x) {
        break;
      }
      graph->addEdge(std::pair(node.id, it_1->second));
      it_1 = comp_by_x_plus_y_map.erase(it_1);
    }
    comp_by_x_plus_y_map[Point(node.x, node.y)] = node.id;
  }
}
} //namespace sc