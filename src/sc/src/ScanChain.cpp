#include <iostream>
#include <fstream>
#include "sc/ScanChain.h"
#include "ZhouSpanningGraphStrategy.h"
#include "Kruskal.h"
#include "Graph.h"
#include "odb/db.h"
#include "utl/Logger.h"
#include "ord/OpenRoad.hh"
#include <lemon/christofides_tsp.h>
#include "ChristofidesSpanningGraph.h"

namespace sc {

using utl::SC;

void ScanChain::init(ord::OpenRoad *openroad) {
  open_road_ = openroad;
  logger_ = open_road_->getLogger();
  db_ = open_road_->getDb();
}

ScanChain::~ScanChain() {
}

void ScanChain::reorder() {
  block_ = db_->getChip()->getBlock();

  for (auto scan_chain : block_->getScanChains()) {

    logger_->info(SC, 1, "Reorder scan chain {}.", scan_chain->getName());

    double length = 0;
    if (!traceScanChain(scan_chain, length)) {
      logger_->warn(SC, 2, "Scan chain {} can't successfully trace, skip reorder.", scan_chain->getName());
      continue;
    }

    std::vector<Point> points;

    initPoints(scan_chain, points);
    logger_->info(SC, 3, "  Has {} points in the graph for scan chain {}.", points.size(), scan_chain->getName());

    std::vector<Graph::NodeId> orders = reorderScanChain(scan_chain, points);
    assert(orders.size() == points.size() && orders[0] == 0);

    //assert the pin of START is output pin
    assert(scan_chain->getStart()->isOutputSignal(false));

    // reconnect the net with scan instances
    orders.pop_back(); //the last element is the stop of the chain, don't need to handle reconnect
    connectScanNet(scan_chain, orders);
  }
}

void ScanChain::initPoints(const odb::dbScanChain *scan_chain, std::vector<Point> &points) {
  auto start_inst = scan_chain->getStart()->getInst();
  int chain_start_x, chain_start_y;
  start_inst->getLocation(chain_start_x, chain_start_y);
  points.push_back(Point(chain_start_x, chain_start_y));

  auto floating_comps = scan_chain->getFloating();
  for (auto comp : floating_comps) {
    int x, y;
    comp->getInst()->getLocation(x, y);
    points.push_back(Point(x, y));
  }

  // transform ordered segment to a single comp
  auto ordered_list = scan_chain->getOrdered();
  for (auto ordered : ordered_list) {
    odb::dbScanChainComp *first_comp = *(ordered->getComps().begin());
    odb::dbScanChainComp *last_comp;
    for (auto comp : ordered->getComps()) {
      last_comp = comp;
    }

    int start_x, start_y, stop_x, stop_y;
    first_comp->getInst()->getLocation(start_x, start_y);
    last_comp->getInst()->getLocation(stop_x, stop_y);
    //TODO it's not a good idea to use middle point
    points.push_back(Point((start_x + stop_x) / 2, (start_y + stop_y) / 2));
  }

  auto stop_inst = scan_chain->getStop()->getInst();
  int chain_stop_x, chain_stop_y;
  stop_inst->getLocation(chain_stop_x, chain_stop_y);
  points.push_back(Point(chain_stop_x, chain_stop_y));
}

void ScanChain::fullGraphReorderScanChain(std::vector<Graph::NodeId>& orders,
                                          const std::vector<Point> &points)
{
  logger_->info(SC, 9, "SpanningGraph don't have a perfect matching so use FullGraph to find a perfect matching");
  lemon::FullGraph g(points.size());
  lemon::FullGraph::EdgeMap<int> tree_map(g);
  for (size_t i = 0; i < points.size(); ++i) {
    for (size_t j = i + 1; j < points.size(); ++j) {
      lemon::FullGraph::Edge edge = g.edge(g(i), g(j));
      double dis = std::abs(points[i].x - points[j].x)
                   + std::abs(points[i].y - points[j].y);
      tree_map[edge] = dis;
    }
  }

  lemon::ChristofidesTsp<lemon::FullGraph::EdgeMap<int>> chris(g, tree_map);
  chris.run();
  std::vector<lemon::FullGraph::Node> nodes = chris.tourNodes();
  auto pos = std::find(nodes.begin(), nodes.end(), g(0));
  for (auto it = pos; it != nodes.end(); ++it) {
    orders.push_back(g.index(*it));
  }
  for (auto it = nodes.begin(); it != pos; ++it) {
    orders.push_back(g.index(*it));
  }
}

std::vector<Graph::NodeId> ScanChain::reorderScanChain(const odb::dbScanChain *scan_chain,
                                                       const std::vector<Point> &points) {

  sc::Graph graph = sc::Graph(points);
  graph.construct(new sc::ZhouSpanningGraphStrategy);

  lemon::ListGraph gr;
  lemon::ListGraph::EdgeMap<int> cost(gr);
  for (size_t i = 0; i < points.size(); ++i) {
    gr.addNode();
  }
  for (size_t i = 0; i < graph.edges_.size(); ++i) {
    sc::Graph::NodeId l = graph.edges_[i].first;
    sc::Graph::NodeId r = graph.edges_[i].second;
    lemon::ListGraph::Edge e = gr.addEdge(gr.nodeFromId(l), gr.nodeFromId(r));
    int dis = (int)graph.vertices_[l].distance(graph.vertices_[r]);
    cost[e] = dis;
  }

  sc::ChristofidesTsp c(gr, cost);
  c.setPoints(points);  
  c.run();
  std::vector<Graph::NodeId> orders = c.tourNodes();
  return orders;
}

odb::dbScanChainComp* getOrigComp(size_t id, bool is_next, std::vector<odb::dbScanChainComp *> orig_floatings,
                                  std::vector<std::pair<odb::dbScanChainComp *, odb::dbScanChainComp *>> orig_ordered) {
  int idx = id - 1; // the start point is skipped
  if (idx < orig_floatings.size()) {
    return orig_floatings[idx];
  } else {
    auto ordered_pair = orig_ordered[idx - orig_floatings.size()];
    if (is_next) {
      return ordered_pair.second;
    } else {
      return ordered_pair.first;
    }
  }
}

void ScanChain::connectScanNet(const odb::dbScanChain *scan_chain,
                               const std::vector<Graph::NodeId> &orders) {
  std::vector<odb::dbScanChainComp *> orig_floatings;
  std::vector<std::pair<odb::dbScanChainComp *, odb::dbScanChainComp *>> orig_ordered;
  auto floating_comps = scan_chain->getFloating();
  for (auto comp : floating_comps) {
    orig_floatings.push_back(comp);
  }
  auto ordered_list = scan_chain->getOrdered();
  for (auto ordered : ordered_list) {
    odb::dbScanChainComp *first_comp = *(ordered->getComps().begin());
    odb::dbScanChainComp *last_comp;
    for (auto comp : ordered->getComps()) {
      last_comp = comp;
    }

    orig_ordered.push_back(std::pair(first_comp, last_comp));
  }

  odb::dbITerm* next_sc_in;
  odb::dbNet* connect_net;
  for (int i = 0; i < orders.size(); ++i) {
    if (i == 0) {
      connect_net = scan_chain->getStart()->getNet();
    } else {
      connect_net = getOrigComp(orders[i], true, orig_floatings, orig_ordered)->getScanOut()->getNet();
    }

    if (i == orders.size() - 1) {
      next_sc_in = scan_chain->getStop();
    } else {
      next_sc_in = getOrigComp(orders[i+1], false, orig_floatings, orig_ordered)->getScanIn();
    }

    next_sc_in->disconnect();
    next_sc_in->connect(connect_net);
  }
}

odb::dbScanChainComp* getCompByITerm(std::map<odb::dbITerm*, odb::dbScanChainComp*> iterm_2_comp, odb::dbITerm* iterm) {
  return (iterm_2_comp.find(iterm) != iterm_2_comp.end()) ? iterm_2_comp[iterm] : nullptr;
}

void ScanChain::trace() {
  block_ = db_->getChip()->getBlock();
  int total_success_count = 0;

  for (auto scan_chain : block_->getScanChains()) {
    double distance = 0;
    if (!traceScanChain(scan_chain, distance)) {
      logger_->warn(SC, 6, "Scan chain {} can't successfully trace.", scan_chain->getName());
    } else {
      total_success_count++;
    }
  }

  logger_->info(SC, 7, "Successfully traced {} scan chains (total {} scan chains).",
                total_success_count, block_->getScanChains().size());

}

bool ScanChain::traceScanChain(const odb::dbScanChain *scan_chain, double& distance) {
  std::map<odb::dbITerm*, odb::dbScanChainComp*> iterm_2_comp;
  for (auto floating : scan_chain->getFloating()) {
    iterm_2_comp[floating->getScanIn()] = floating;
  }
  for (auto ordered : scan_chain->getOrdered()) {
    for (auto comp : ordered->getComps()) {
      iterm_2_comp[comp->getScanIn()] = comp;
    }
  }

  distance = 0;
  int x,y;
  scan_chain->getStart()->getInst()->getLocation(x, y);
  Point pre_point = Point(x, y);

  odb::dbITerm* out = scan_chain->getStart();
  int inst_count = 0;
  while (out != nullptr) {
    auto net = out->getNet();
    if (net == nullptr) {
      return false;
    }

    odb::dbSet<odb::dbITerm> iterms = net->getITerms();
    out = nullptr;

    for (auto iterm : iterms) {
      // check input
      if (!iterm->isInputSignal(false)) {
        continue;
      }

      if (iterm == scan_chain->getStop()) {
        if (inst_count != iterm_2_comp.size()) {
          logger_->warn(SC, 8, "Scan chain {} is incomplete in net format.", scan_chain->getName());
          return false;
        }
        scan_chain->getStop()->getInst()->getLocation(x, y);
        Point cur_point = Point(x, y);
        distance += pre_point.distance(cur_point);

        logger_->info(SC, 4, "Total wire length of scan chain {} is {}.", scan_chain->getName(), distance);
        logger_->info(SC, 5, "Successfully traced scan chain {}, total {} scan bits.", scan_chain->getName(), inst_count);
        return true;
      }

      // whether scan input
      auto comp = getCompByITerm(iterm_2_comp, iterm);
      if (comp == nullptr) {
        continue;
      }
      inst_count ++;

      comp->getInst()->getLocation(x, y);
      Point cur_point = Point(x, y);
      distance += pre_point.distance(cur_point);
      pre_point = cur_point;

      out = comp->getScanOut();
    }
  }

  return false;
}

void ScanChain::reportScanChain(const char *filename) {
  if (filename == nullptr) {
    return;
  }

  std::ofstream file(filename);
  file.setf(std::ios::fixed);
  file.precision(0);
  if (!file.is_open()) {
    logger_->error(SC, 8, "Could not open output metric file {}.", filename);
  }

  block_ = db_->getChip()->getBlock();
  auto scan_chains = block_->getScanChains();
  int total_success_count = 0;
  double total_length = 0;

  file << "{ \"scan_chains\":" << scan_chains.size()
       << ", \"length\": {";

  for (auto iter = scan_chains.begin(); iter != scan_chains.end();) {
    auto scan_chain = *iter;
    double length = 0;
    if (traceScanChain(scan_chain, length)) {
      total_success_count++;
      total_length = total_length + length;
      file << "\"" << scan_chain->getName() << "\":" << length;
    }

    if (++iter != scan_chains.end())
      file << ",";
  }
  file << "}"
       << ", \"total_length\": " << total_length
       << ", \"correct_scan_chains\": " << total_success_count
       << "}\n";
}
} // namespace sc