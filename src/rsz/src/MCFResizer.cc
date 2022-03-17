#include "gui/gui.h"
#include "ord/OpenRoad.hh"
#include "rsz/LPResizer.hh"

namespace rsz {

using namespace operations_research;
using namespace sta;

////////////////////////////////////////////////////////
//
//  class MCFResizerCore
//
////////////////////////////////////////////////////////

MCFResizerCore::MCFResizerCore(OpenRoad* openroad, LPOptions* options)
    : Resizer(), options_(options)
{
  init(openroad,
       openroad->tclInterp(),
       openroad->getLogger(),
       // Broken gui api missing openroad accessor.
       gui::Gui::get(),
       openroad->getDb(),
       openroad->getSta(),
       openroad->getSteinerTreeBuilder(),
       openroad->getGlobalRouter());
  init();
  initMCF();
}

Index MCFResizerCore::getIndex(Vertex* vertex) const
{
  std::string vertex_name = vertex->name(sta_->sdcNetwork());
  if (!vertex_index_map_.hasKey(vertex_name))
    return MCF_INVALIDE;
  return vertex_index_map_.at(vertex_name);
}

std::string MCFResizerCore::getNodeVertexName(Index index) const
{
  for (auto row : vertex_index_map_)
    if (row.second == index)
      return row.first;
  return "unknown_node_name";
}

Delay MCFResizerCore::getDelay(Vertex* vertex)
{
  std::string vertex_name = vertex->name(sta_->sdcNetwork());
  Delay load_delay = LP_ZERO;
  PathRef path = sta_->vertexWorstSlackPath(vertex, max_);
  PathExpanded expanded(&path, sta_);
  const DcalcAnalysisPt* dcalc_ap = path.dcalcAnalysisPt(sta_);
  int start_index = expanded.startIndex();
  int path_length = expanded.size();
  for (int i = start_index; i < path_length; i++) {
    PathRef* path = expanded.path(i);
    Vertex* path_vertex = path->vertex(sta_);
    if (vertex == path_vertex) {
      TimingArc* prev_arc = expanded.prevArc(i);
      Edge* prev_edge = path->prevEdge(prev_arc, sta_);
      if (!prev_edge)
        continue;
      load_delay
          = sta_->graph()->arcDelay(prev_edge, prev_arc, dcalc_ap->index());
      debugPrint(logger_,
                 utl::RSZ,
                 "LPHoldFixer",
                 4,
                 "{} load_delay = {}",
                 path_vertex->name(sta_->network()),
                 delayAsString(load_delay, sta_, 3));
    }
  }
  return load_delay;
}

Delay MCFResizerCore::getLowBound(Vertex* vertex)
{
  // todo
  return 0.01 / LP_STA_SCALING;
}
Delay MCFResizerCore::getHighBound(Vertex* vertex)
{
  // todo
  return 1.00 / LP_STA_SCALING;
}

void MCFResizerCore::buildIndexMap(VertexSet ends)
{
  Index index = s_index_;
  VertexSet all_ends = getAllRelatedEnds(ends, options_->ignoreClk());
  VertexSet fanins = findHoldFanins(all_ends);
  VertexSeq sorted_fanins = sortHoldFanins(fanins);

  vertex_index_map_.insert(MCF_SRC_NAME, index);
  for (auto vertex : sorted_fanins) {
    index++;
    std::string vertex_name = vertex->name(sta_->sdcNetwork());
    vertex_index_map_.insert(vertex_name, index);
  }
}

void MCFResizerCore::addConstraintBase(Index pred,
                                       Index succ,
                                       Cost capacity,
                                       Cost cost)
{
  start_nodes_.push_back(pred);
  end_nodes_.push_back(succ);
  capacities_.push_back(capacity);
  unit_costs_.push_back(cost);
}

void MCFResizerCore::addConstraintEdge(Vertex* pred, Vertex* succ)
{
  Delay delay = getDelay(succ);
  Index pred_index = getIndex(pred);
  Index succ_index = getIndex(succ);
  Cost capacity = delay2Cost(delay);  // todo
  Cost low_bound = getLowBound(succ);
  Cost high_bound = getHighBound(succ);
  addConstraintBase(pred_index, succ_index, capacity, -delay2Cost(delay));
  addConstraintBase(
      pred_index, succ_index, MCF_INT_INF, -delay2Cost(low_bound));
  addConstraintBase(
      succ_index, pred_index, MCF_INT_INF, delay2Cost(high_bound));
}

void MCFResizerCore::addConstraintPO(Vertex* po)
{
  const Slack slack_margin = options_->slackMargin() / LP_STA_SCALING;
  Index po_index = getIndex(po);
  Required req = sta_->vertexRequired(po, MinMax::max()) + slack_margin;
  addConstraintBase(po_index, s_index_, MCF_INT_INF, delay2Cost(req));
  solver_.SetNodeSupply(po_index, MCF_INT_INF);
}

void MCFResizerCore::addConstraintPI(Vertex* pi)
{
  Index pi_index = getIndex(pi);
  addConstraintBase(s_index_, pi_index, MCF_INT_INF, MCF_ZERO);
}

void MCFResizerCore::initMCF()
{
  const Slack slack_margin = options_->slackMargin() / LP_STA_SCALING;
  VertexSet* ends = sta_->search()->endpoints();
  VertexSet failures;
  for (Vertex* vertex : *ends) {
    Slack slack = sta_->vertexSlack(vertex, MinMax::max());
    if (!fuzzyLess(slack, slack_margin))
      failures.insert(vertex);
  }
  buildIndexMap(failures);
  // build all constraints
}

MCFResult MCFResizerCore::doMinCostFlow()
{
  // Add each arc.
  for (int i = 0; i < start_nodes_.size(); ++i)
    solver_.AddArcWithCapacityAndUnitCost(
        start_nodes_[i], end_nodes_[i], capacities_[i], unit_costs_[i]);

  // Add node supplies.
  solver_.SetNodeSupply(s_index_, MCF_INT_INF);

  return solver_.SolveMaxFlowWithMinCost();
}

MCFEdgeWeightSet MCFResizerCore::buildBellmanGraph() const
{
  MCFEdgeWeightSet bellman_graph;
  for (std::size_t i = 0; i < solver_.NumArcs(); ++i) {
    Cost flow = solver_.Flow(i);
    Cost capacity = solver_.Capacity(i);
    Index pred = solver_.Tail(i);
    Index succ = solver_.Head(i);
    Delay delay = -cost2Delay(solver_.UnitCost(i));

    // build Bellman Graph with KKT condition for edge (i,j)
    // A(i) + w(i,j) >= A(j); f(i,j) < c(i,j)
    if (flow > MCF_ZERO){}
      bellman_graph.insert(MCFEdgeWeight(MCFEdge(pred,succ), delay));
    // A(i) <= A(j) - w(i,j); f(i,j) > 0
    if (flow < capacity || isInf(capacity))
      bellman_graph.insert(MCFEdgeWeight(MCFEdge(succ, pred), -delay));
  }
  return bellman_graph;
}

void MCFResizerCore::initArrivalMap()
{
  arrival_map_.clear();
  for (auto node : vertex_index_map_) {
    Index index = node.second;
    if (index == s_index_)
      arrival_map_.insert(index, LP_ZERO);
    else
      arrival_map_.insert(index, LP_INFINITY);
  }
}

bool MCFResizerCore::updateArrivalMap(const MCFEdgeWeightSet bellman_graph)
{
  bool changed = false;
  for (auto edge : bellman_graph) {
    Index pred = edge.first.first;
    Index succ = edge.first.second;
    Delay weight = edge.second;
    Arrival pred_arrival = arrival_map_[pred];
    Arrival succ_arrival = arrival_map_[succ];
    Arrival new_arrival = pred_arrival + weight;
    if (!(fuzzyEqual(LP_INFINITY, pred_arrival) && fuzzyLess(LP_ZERO, weight))
        && fuzzyLess(new_arrival, succ_arrival)) {
      arrival_map_[succ] = new_arrival;
      changed = true;
    }
  }
  return changed;
}

bool MCFResizerCore::remap()
{
  // Bellman Ford Shortest Path Algorithm
  bool changed = true;
  MCFEdgeWeightSet bellman_graph = buildBellmanGraph();
  initArrivalMap();
  for (size_t iter = 0; iter < arrival_map_.size() && changed; ++iter)
    changed = updateArrivalMap(bellman_graph);

  if (changed)
    logger_->warn(utl::RSZ, 1200, "Bellman Graph infeasible");

  return !changed;
}

Delay MCFResizerCore::getNewDelay(Vertex* vertex)
{
  Delay shortest_delay = LP_INFINITY;
  VertexInEdgeIterator edge_iter(vertex, sta_->graph());
  while (edge_iter.hasNext()) {
    Edge* edge = edge_iter.next();
    Vertex* fanin = edge->from(sta_->graph());
    VertexInEdgeIterator pred_edge_iter(fanin, sta_->graph());
    while (pred_edge_iter.hasNext()) {
      Edge* pred_edge = pred_edge_iter.next();
      Vertex* pred = pred_edge->from(sta_->graph());
      if (vertex_index_map_.hasKey(pred->name(sta_->sdcNetwork()))) {
        Index pred_index = getIndex(pred);
        Index succ_index = getIndex(vertex);
        Delay new_delay = arrival_map_[succ_index] - arrival_map_[pred_index];
        if (fuzzyLess(new_delay, shortest_delay))
          shortest_delay = new_delay;
      }
    }
  }
  return shortest_delay;
}

Delay MCFResizerCore::getResizeValue(Vertex* vertex)
{
  return getNewDelay(vertex) - getDelay(vertex);
}

VertexSet MCFResizerCore::getResizeVertex()
{
  VertexSet resize_vertex;
  for (auto node : vertex_index_map_) {
    std::string name = node.first;
    if (name.compare(MCF_SRC_NAME)) {
      Vertex* vertex = findRealVertex(name);
      Delay resize_value = getResizeValue(vertex);
      if (!fuzzyZero(resize_value))
        resize_vertex.insert(vertex);
    }
  }
  return resize_vertex;
}

}  // namespace rsz