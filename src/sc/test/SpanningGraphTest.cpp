#include "testa.hpp"
#include <set>
#include "ConstructStrategy.h"
#include "CompleteGraphStrategy.h"
#include "ZhouSpanningGraphStrategy.h"
#include "Kruskal.h"

using namespace std;

namespace {

using namespace sc;

vector<Point> generatePoints(int n) {
  set<string> set;
  vector<Point> result;

  int kMin = 1;
  int kMax = 10000000;

  for (int i = 0; i < n; i++) {
    Point point;
    string point_str;
    do {
      point = Point(rand() % (kMax - kMin + 1) + kMin, rand() % (kMax - kMin + 1) + kMin);
      point_str = to_string(point.x) + "_" + to_string(point.y);
    } while (set.count(point_str));
    set.insert(point_str);
    result.push_back(point);
  }
  return result;
}


void graph_test (const string&) {

  vector<Point> points = generatePoints(1000);

  auto graph = Graph(points);
  graph.construct(new CompleteGraphStrategy);
  auto min_graph = Graph(graph.vertices_);
  min_graph.construct(new Kruskal(graph.edges_));
  auto orders_complete = min_graph.dfs(0);

  graph = Graph(points);
  graph.construct(new ZhouSpanningGraphStrategy);
  min_graph = Graph(graph.vertices_);
  min_graph.construct(new Kruskal(graph.edges_));
  auto orders_spanning = min_graph.dfs(0);

  TESTA_ASSERT(orders_complete == orders_spanning).issue("failure");
}

} // namespace
TESTA_DEF_JUNIT_LIKE1(graph_test);