#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

#include "CtsOptions.h"
#include "TreeBuilder.h"

namespace utl {
class Logger;
}  // namespace utl

namespace lrl {
class DelayCalc;
} // namespace lrl

namespace cts {

const double eps = 1e-7;

class GridPoint {
 public:
  double x, y;
  GridPoint(double x_, double y_) : x(x_), y(y_) {}

  double z() const { return x + y; }
  double w() const { return x - y; }
  bool operator==(const GridPoint& rhs) const { return std::abs(x - rhs.x) < eps && std::abs(y - rhs.y) < eps; }
  bool operator<(const GridPoint& rhs) const {
    if (x != rhs.x) {
      return x < rhs.x;
    } else {
      return y < rhs.y;
    }
  }
  friend inline std::ostream& operator<<(std::ostream& os, const GridPoint& gp) {
    os << "(" << gp.x << "," << gp.y << ")";
    return os;
  }
};

class Segment {
 public:
  GridPoint p1, p2;  // p1 has lower y
  double delay;
  double distance;
  float cap;
  float slew_degradation;
  odb::dbMaster* buffer;
  std::vector<odb::dbMaster*> more_buffers;
  int id = 0;  // unique id for each segment
  bool gate_cell;
  ClockInst* sinkInst;
  int fanout = 0;

  Segment(GridPoint u, GridPoint v) : p1(u), p2(v) {
    delay = 0;
    slew_degradation = 0;
    cap = 0;
    distance = 0;
    buffer = nullptr;
    gate_cell = false;
    sinkInst = nullptr;
    if (p1.y > p2.y) {
      std::swap(p1, p2);
    }
  }

  bool isLeaf() const { return p1 == p2; }
  bool operator==(const Segment& rhs) const { return (p1 == rhs.p1 && p2 == rhs.p2); }
  size_t hash() const {
    int p1x = p1.x * 100;
    int p1y = p1.y * 100;
    int p2x = p2.x * 100;
    int p2y = p2.y * 100;

    size_t res = p1x ^ (p2x << 16);
    res <<= 32;
    res += p1y ^ (p2y << 16);
    return res;
  }
  friend inline std::ostream& operator<<(std::ostream& os, const Segment& seg) {
    os << "Seg: (" << seg.p1 << "," << seg.p2 << ")";
    return os;
  }

  std::string str() const {
    return std::to_string(p1.x) + "," +  std::to_string(p1.y) +
           ";" + std::to_string(p2.x) + "," +  std::to_string(p2.y);
  }

  double slope() {
    if (isLeaf()) {
      return 0;
    }
    if (p2.x == p1.x) {
      return 0;
    }
    return (p1.y - p2.y) / (p1.x - p2.x);
  }

  Segment* intersect(Segment& rhs) {
    double cur_slope = slope();
    double rhs_slope = rhs.slope();
    Segment* ret = nullptr;

    // current segment intersect with single point
    if (rhs.isLeaf()) {
      if (std::abs((rhs.p1.y - p1.y) * (p2.x - p1.x) - (p2.y - p1.y) * (rhs.p1.x - p1.x)) < eps) {
        if (p1.y-eps <= rhs.p1.y && rhs.p1.y <= p2.y+eps) {  // valid intersection
          ret = new Segment(rhs.p1, rhs.p2);
          return ret;
        }
      }
    }

    if (isLeaf()) {
      return rhs.intersect(*this);
    }

    if (std::abs(cur_slope - rhs_slope) < eps) {
      if (std::abs((rhs.p1.y - p1.y) * (p2.x - p1.x) - (p2.y - p1.y) * (rhs.p1.x - p1.x)) < eps) {
        GridPoint* upper;
        GridPoint* lower;
        if (rhs.p2.y < p2.y) {
          upper = &rhs.p2;
        } else {
          upper = &p2;
        }
        if (rhs.p1.y > p1.y) {
          lower = &rhs.p1;
        } else {
          lower = &p1;
        }
        if (upper->y >= lower->y) {
          ret = new Segment(*lower, *upper);
          return ret;
        }
      }
    } else {
      // y=x+a y=-x+b
      // y=(a+b)/2
      // x=(b-a)/2
      double a = 0;
      double b = 0;
      if(std::abs(cur_slope - 1) < eps) {
        a = p1.y - p1.x;
        b = rhs.p1.x + rhs.p1.y;
      }
      if(std::abs(cur_slope + 1) < eps) {
        a = rhs.p1.y - rhs.p1.x;
        b = p1.x + p1.y;
      }

      double x = (b-a)/2.0;
      double y = (a+b)/2.0;

      if (y >= p1.y-eps && y <= p2.y+eps && y>= rhs.p1.y-eps && y <= rhs.p2.y+eps) {  // valid intersection
        ret = new Segment(GridPoint(x, y), GridPoint(x, y));
        return ret;
      }
    }
    return ret;
  }
};

class TreeNode {
 public:
  int id;
  Segment* segment;
  GridPoint* loc;
  std::vector<TreeNode*> children;
  TreeNode* par;
  TreeNode(int id_) : id(id_) {
    par = nullptr;
    segment = nullptr;
    loc = nullptr;
  }

  ~TreeNode() {
    delete segment;
    delete loc;
  }
};

class TreeTopology {
 public:
  TreeNode* root;
  int size;
  std::unordered_map<int, TreeNode*> id_node;

  ~TreeTopology() {
    for (auto entry : id_node) {
      delete entry.second;
    }
  }
};

class TRR {
 public:
  Segment* core;
  double radius;
  TRR(Segment* seg, double radi) : core(seg), radius(radi) {}
  friend inline std::ostream& operator<<(std::ostream& os, const TRR& trr) {
    os << *trr.core << "; radius:" << trr.radius;
    return os;
  }
  Segment* intersect(Segment& seg) {
    std::vector<GridPoint> trr_boundary_grid;
    std::vector<Segment> trr_Sides;
    trr_boundary_grid.emplace_back(core->p1.x, core->p1.y - radius);
    trr_boundary_grid.emplace_back(core->p1.x + radius, core->p1.y);
    trr_boundary_grid.emplace_back(core->p1.x, core->p1.y + radius);
    for (int i = 0; i < 3; i++) {
      trr_Sides.emplace_back(trr_boundary_grid[i], trr_boundary_grid[i + 1]);
    }
    trr_Sides.emplace_back(trr_boundary_grid[3], trr_boundary_grid[0]);
    for (auto& side : trr_Sides) {
      Segment* intersection = side.intersect(seg);
      if (intersection != nullptr) {
        return intersection;
      }
    }
    return nullptr;
  }
};

class Edge {
 public:
  double cost;
  std::vector<Segment*> nodes;

  bool depended(Edge& edge) {
    if (nodes[0] == edge.nodes[0] || nodes[0] == edge.nodes[1]) {
        if (nodes[1] == edge.nodes[0] || nodes[1] == edge.nodes[1]) {
            return false; //two edges share the same nodes
          } else {
          return true; //two edges share only one node
        }
      }
    if (edge.nodes[0] == nodes[0] || edge.nodes[0] == nodes[1]) {
        if (edge.nodes[1] == nodes[0] || edge.nodes[1] == nodes[1]) {
            return false; //two edges share the same nodes
          } else {
          return true; //two edges share only one node
        }
      }

    // two edges share no node
    return false;
  }
};


class BufferSolution {
 public:
  int node_id;
  float slew;
  float delay;
  float cap;
  float power;
  Segment loc;
  std::string left_buf_name;
  std::string right_buf_name;
  std::deque<BufferSolution> subSolutions;

  BufferSolution(int node_id_, float slew_, float delay_, float cap_, Segment loc_)
  : node_id(node_id_), slew(slew_), delay(delay_), cap(cap_), loc(loc_){};
};
//-----------------------------------------------------------------------------
class NNCBuilder : public TreeBuilder {

 public:
  double source_delay;
  double source_cap;

  NNCBuilder(CtsOptions* options,
             Clock& net,
             TreeBuilder* parent,
             sta::dbSta* sta,
             sta::dbNetwork* network,
             utl::Logger* logger)
      : TreeBuilder(options, net, parent), _openSta(sta), _network(network), logger_(logger){};

  void run();

 protected:
  sta::dbSta* _openSta;
  sta::dbNetwork* _network;
  utl::Logger* logger_;

  int graph_k = 2;
  float wire_slew_coefficient = log(9);

  void relationGraphByBucket(std::vector<Segment*> segs, std::vector<Edge>& edges, bool skip_by_delay);
  void findIndependentSet(std::vector<Edge>& edges, int K, int k, std::vector<Edge>& indep_edges);
  TreeTopology* constructTopo(std::vector<Segment*>& segments);
  Segment* mergeByDelay(Segment* seg1, Segment* seg2);
  double calcMergePointByElmore(const double seg1_delay, const double seg2_delay,
                                const double seg1_cap, const double seg2_cap,
                                double d, double& d1, double& d2,
                                double& wire_d1, double& wire_d2);
  void buildSolution(TreeTopology* tree);
  void plotSolution(TreeTopology* tree);
  bool skipMergeByDelay(Segment* g1, Segment* g2, double distance);
  void buildSolutionInner(TreeNode* node,
                          std::queue<TreeNode*>& buffer_queue,
                          std::vector<TreeNode*>& net_nodes);
  void localEmbedding(TreeNode* node, int& node_count, int& sink_count);
  void insertBuffer(Segment* seg, odb::dbMaster* buffer);
  void fixCapByBuffer(Segment* seg1, Segment* seg2, double distance, double& p_cap);
  void fixMaxFanoutByBuffer(Segment* seg1, Segment* seg2, double distance,
                            double& p_cap, int& p_fanout);
  void balanceDelayByBuffer(Segment* seg1, Segment* seg2,
                            double d, double& d1, double& d2,
                            double& wire_delay1, double& wire_delay2);
};

}  // namespace cts
