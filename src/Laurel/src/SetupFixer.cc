#include "gui/gui.h"
#include "lrl/Laurel.hh"
#include "ord/OpenRoad.hh"

namespace lrl {

using namespace operations_research;
using namespace sta;

////////////////////////////////////////////////////////
//
//  class SetupFixer
//
////////////////////////////////////////////////////////

SetupFixer::SetupFixer(OpenRoad* openroad, Options* options)
    : LaurelAdapter(), options_(options)
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
  resizePreamble();
  init();
  // initAlgorithm();
}

Cost SetupFixer::delay2Cost(Delay delay)
{
  Delay scaled_delay = std::stof(delayAsString(delay, sta_, 3));
  return round(scaled_delay * LRL_H_SCALING);
}
Delay SetupFixer::cost2Delay(Cost cost) const
{
  return ((Delay) cost) / LRL_H_SCALING / LRL_H_STA_SCALING;
}

Index SetupFixer::getIndex(Vertex* vertex) const
{
  Name vertex_name = vertex->name(sta_->sdcNetwork());
  if (!vertex_index_map_.hasKey(vertex_name))
    return LRL_S_INVALIDE;
  return vertex_index_map_.at(vertex_name);
}

Name SetupFixer::getNodeVertexName(Index index) const
{
  for (auto row : vertex_index_map_)
    if (row.second == index)
      return row.first;
  return "unknown_node_name";
}

Delay SetupFixer::getDelay(Vertex* vertex)
{
  if (!vertex->isDriver(sta_->network()))
    return LRL_H_ZERO;
  Pin* drvr_pin = vertex->pin();
  LibertyPort* drvr_port = sta_->network()->libertyPort(drvr_pin);
  PathRef path = sta_->vertexWorstSlackPath(vertex, max_);
  if(!path.vertex(sta_))
    return LRL_H_ZERO;
  const DcalcAnalysisPt* dcalc_ap = path.dcalcAnalysisPt(sta_);
  const Delay load_cap = graph_delay_calc_->loadCap(drvr_pin, dcalc_ap);
  return gateDelay(drvr_port, load_cap, dcalc_ap);
}

const char* SetupFixer::getInPortName(Vertex* pred, Vertex* succ) const
{
  VertexOutEdgeIterator pred_edge_iter(pred, sta_->graph());
  while (pred_edge_iter.hasNext()) {
    Edge* pred_edge = pred_edge_iter.next();
    Vertex* fanout = pred_edge->to(sta_->graph());
    VertexInEdgeIterator succ_edge_iter(succ, sta_->graph());
    while (succ_edge_iter.hasNext()) {
      Edge* succ_edge = succ_edge_iter.next();
      Vertex* fanin = succ_edge->from(sta_->graph());
      if (fanin == fanout) {
        Pin* pin = fanin->pin();
        LibertyPort* port = sta_->network()->libertyPort(pin);
        return port->name();
      }
    }
  }
  return nullptr;
}

Delay SetupFixer::getBound(Vertex* vertex, const MinMax* min_max)
{
  if (!vertex->isDriver(sta_->network()))
    return LRL_H_ZERO;
  Pin* drvr_pin = vertex->pin();
  LibertyPort* drvr_port = sta_->network()->libertyPort(drvr_pin);
  LibertyCell* cell = drvr_port->libertyCell();
  LibertyCellSeq* equiv_cells = sta_->equivCells(cell);
  PathRef path = sta_->vertexWorstSlackPath(vertex, max_);
  const DcalcAnalysisPt* dcalc_ap = path.dcalcAnalysisPt(sta_);
  const char* drvr_port_name = drvr_port->name();
  const Delay load_cap = graph_delay_calc_->loadCap(drvr_pin, dcalc_ap);
  int lib_ap = dcalc_ap->libertyIndex();
  Delay extreme_delay = INF;
  if (!equiv_cells) {
    debugPrint(logger_, utl::LRL, "LRL", 3, "no equiv_cells");
    return LRL_H_ZERO;
  }
  for (LibertyCell* equiv : *equiv_cells) {
    LibertyCell* equiv_corner = equiv->cornerCell(lib_ap);
    LibertyPort* equiv_drvr = equiv_corner->findLibertyPort(drvr_port_name);
    Delay equiv_delay = gateDelay(equiv_drvr, load_cap, dcalc_ap);
    if (((min_max == MinMax::max()) ^ fuzzyLess(equiv_delay, extreme_delay))
        || fuzzyInf(extreme_delay))
      extreme_delay = equiv_delay;
  }
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             3,
             "{} extreme delay {}",
             vertex->name(sta_->sdcNetwork()),
             delayAsString(extreme_delay, sta_, 3));
  return extreme_delay;
}

Delay SetupFixer::getLowBound(Vertex* vertex)
{
  return getBound(vertex, MinMax::min());
}

Delay SetupFixer::getHighBound(Vertex* vertex)
{
  // todo
  return getBound(vertex, MinMax::max());
}

Index SetupFixer::buildIndex(Vertex* vertex)
{
  Name vertex_name = vertex->name(sta_->sdcNetwork());
  if (!vertex_index_map_.hasKey(vertex_name))
    vertex_index_map_.insert(vertex_name, index_++);
  return vertex_index_map_[vertex_name];
}

void SetupFixer::buildAllFaninEdge(VertexSeq fanins)
{
  for (auto vertex : fanins) {
    bool is_pi = true;
    buildIndex(vertex);
    SearchPredNonReg2 pred(sta_);
    BfsBkwdIterator iter(BfsIndex::other, &pred, this);
    iter.enqueueAdjacentVertices(vertex);
    while (iter.hasNext()) {
      Vertex* fanin = iter.next();
      if (!fanin->isDriver(sta_->network()))
        iter.enqueueAdjacentVertices(fanin);
      else {
        buildIndex(fanin);
        addConstraintEdge(fanin, vertex);
        is_pi = false;
      }
    }
    if (is_pi)
      addConstraintPI(vertex);
  }
}
void SetupFixer::buildALLEdgePO(VertexSet ends)
{
  for (auto node : vertex_index_map_) {
    Vertex* vertex = findRealVertex(node.first);
    if (ends.find(vertex) != ends.end())
      addConstraintPO(vertex);
  }
}

void SetupFixer::buildIndexMap(VertexSet ends)
{
  VertexSet all_ends = getAllRelatedEnds(ends, options_->ignoreClk());
  VertexSet fanins = findHoldFanins(all_ends);
  VertexSeq sorted_ends = sortHoldFanins(all_ends);
  VertexSeq sorted_fanins = sortHoldFanins(fanins);

  // the special node SRC
  vertex_index_map_.insert(LRL_S_SRC_NAME, index_++);

  buildAllFaninEdge(sorted_ends);
  buildAllFaninEdge(sorted_fanins);
  buildALLEdgePO(all_ends);
}

void SetupFixer::addConstraintBase(Index pred,
                                   Index succ,
                                   Cost capacity,
                                   Cost cost)
{
  start_nodes_.push_back(pred);
  end_nodes_.push_back(succ);
  capacities_.push_back(capacity);
  unit_costs_.push_back(cost);
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             4,
             "cst: {} -> {} , cap = {}, cost = {}",
             getNodeVertexName(pred),
             getNodeVertexName(succ),
             isInf(capacity) ? "INF" : std::to_string(capacity),
             std::to_string(cost));
}

Delay SetupFixer::costSlope(Vertex* pred, Vertex* succ)
{
  // todo
  return 1.0f;
}

void SetupFixer::addConstraintEdge(Vertex* pred, Vertex* succ)
{
  Delay delay = getDelay(succ);
  Index pred_index = getIndex(pred);
  Index succ_index = getIndex(succ);
  Cost capacity = delay2Cost(costSlope(pred, succ) * delay);
  Delay low_bound = getLowBound(succ);
  Delay high_bound = getHighBound(succ);
  addConstraintBase(pred_index, succ_index, capacity, -delay2Cost(delay));
  addConstraintBase(
      pred_index, succ_index, LRL_S_INT_INF, -delay2Cost(low_bound));
  addConstraintBase(
      succ_index, pred_index, LRL_S_INT_INF, delay2Cost(high_bound));
}

void SetupFixer::addConstraintPO(Vertex* po)
{
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  Index po_index = getIndex(po);
  Required req = sta_->vertexRequired(po, MinMax::max()) + slack_margin;
  addConstraintBase(po_index, S_INDEX, LRL_S_INT_INF, delay2Cost(req));
  solver_.SetNodeSupply(po_index, -LRL_S_INT_INF);
}

void SetupFixer::addConstraintPI(Vertex* pi)
{
  Index pi_index = getIndex(pi);
  addConstraintBase(S_INDEX, pi_index, LRL_S_INT_INF, LRL_S_ZERO);
}

void SetupFixer::initAlgorithm()
{
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  VertexSet* ends = sta_->search()->endpoints();
  VertexSet failures;

  logger_->info(utl::LRL,
                1302,
                "Violated Ends: (slack_margin = {})",
                delayAsString(slack_margin, sta_, 3));
  for (Vertex* vertex : *ends) {
    Slack slack = sta_->vertexSlack(vertex, MinMax::max());
    if (!fuzzyInf(slack) && fuzzyLess(slack, slack_margin)) {
      failures.insert(vertex);
      logger_->info(utl::LRL,
                    1303,
                    "   {} : {}",
                    delayAsString(slack, sta_, 3),
                    vertex->name(sta_->sdcNetwork()));
    }
  }
  buildIndexMap(failures);
}

SetupResult SetupFixer::resolve()
{
  // Add each arc.
  for (int i = 0; i < start_nodes_.size(); ++i)
    solver_.AddArcWithCapacityAndUnitCost(
        start_nodes_[i], end_nodes_[i], capacities_[i], unit_costs_[i]);

  // Add node supplies.
  // solver_.SetNodeSupply(S_INDEX, LRL_S_INT_INF);

  return solver_.SolveMaxFlowWithMinCost();
}

IdEdgeWeightSet SetupFixer::buildBellmanGraph() const
{
  IdEdgeWeightSet bellman_graph;
  for (std::size_t i = 0; i < solver_.NumArcs(); ++i) {
    Cost flow = solver_.Flow(i);
    Cost capacity = solver_.Capacity(i);
    Index pred = solver_.Tail(i);
    Index succ = solver_.Head(i);
    Delay delay = -cost2Delay(solver_.UnitCost(i));

    // build Bellman Graph with KKT condition for edge (i,j)
    // A(i) + w(i,j) >= A(j); f(i,j) < c(i,j)
    if (flow > LRL_S_ZERO)
      bellman_graph.insert(IdEdgeWeight(IdEdge(pred, succ), delay));
    // A(i) <= A(j) - w(i,j); f(i,j) > 0
    if (flow < capacity || isInf(capacity))
      bellman_graph.insert(IdEdgeWeight(IdEdge(succ, pred), -delay));
  }
  return bellman_graph;
}

void SetupFixer::initArrivalMap()
{
  arrival_map_.clear();
  for (auto node : vertex_index_map_) {
    Index index = node.second;
    if (index == S_INDEX)
      arrival_map_.insert(index, LRL_H_ZERO);
    else
      arrival_map_.insert(index, LRL_H_INFINITY);
  }
}

bool SetupFixer::updateArrivalMap(const IdEdgeWeightSet bellman_graph)
{
  bool changed = false;
  for (auto edge : bellman_graph) {
    Index pred = edge.first.first;
    Index succ = edge.first.second;
    Delay weight = edge.second;
    Arrival pred_arrival = arrival_map_[pred];
    Arrival succ_arrival = arrival_map_[succ];
    Arrival new_arrival = pred_arrival + weight;
    if (!(fuzzyEqual(LRL_H_INFINITY, pred_arrival)
          && fuzzyLess(LRL_H_ZERO, weight))
        && fuzzyLess(new_arrival, succ_arrival)) {
      arrival_map_[succ] = new_arrival;
      changed = true;
    }
  }
  return changed;
}

bool SetupFixer::remap()
{
  // Bellman Ford Shortest Path Algorithm
  bool changed = true;
  IdEdgeWeightSet bellman_graph = buildBellmanGraph();
  initArrivalMap();
  for (size_t iter = 0; iter <= arrival_map_.size() && changed; ++iter)
    changed = updateArrivalMap(bellman_graph);

  if (changed)
    logger_->warn(utl::LRL, 1200, "Bellman Graph infeasible");

  return !changed;
}

Delay SetupFixer::getNewDelay(Vertex* vertex)
{
  Delay shortest_delay = LRL_H_INFINITY;
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
        if (pred_index != S_INDEX) {
          Delay new_delay = arrival_map_[succ_index] - arrival_map_[pred_index];
          if (fuzzyLess(new_delay, shortest_delay))
            shortest_delay = new_delay;
        }
      }
    }
  }
  if (shortest_delay == LRL_H_INFINITY)
    shortest_delay = LRL_H_ZERO;
  return shortest_delay;
}

Delay SetupFixer::getResizeValue(Vertex* vertex)
{
  Delay new_delay = getNewDelay(vertex);
  if (fuzzyZero(new_delay))
    return LRL_H_ZERO;
  return new_delay - getDelay(vertex);
}

VertexSet SetupFixer::getResizeVertex()
{
  VertexSet resize_vertex;
  for (auto node : vertex_index_map_) {
    Name name = node.first;
    if (name.compare(LRL_S_SRC_NAME)) {
      Vertex* vertex = findRealVertex(name);
      Delay resize_value = getResizeValue(vertex);
      if (!fuzzyZero(resize_value))
        resize_vertex.insert(vertex);
    }
  }
  return resize_vertex;
}

void SetupFixer::report()
{
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             3,
             "Minimum cost flow: {}",
             std::to_string(solver_.OptimalCost()));
  for (std::size_t i = 0; i < solver_.NumArcs(); ++i) {
    Index pred = solver_.Tail(i);
    Index succ = solver_.Head(i);
    Cost flow = solver_.Flow(i);
    Cost cap = solver_.Capacity(i);
    Cost cost = solver_.UnitCost(i);
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               3,
               "{} -> {}, flow = {}/{}, cost = {}",
               getNodeVertexName(pred),
               getNodeVertexName(succ),
               isInf(flow) ? "INF" : std::to_string(flow),
               isInf(cap) ? "INF" : std::to_string(cap),
               std::to_string(cost));
  }
}

}  // namespace lrl