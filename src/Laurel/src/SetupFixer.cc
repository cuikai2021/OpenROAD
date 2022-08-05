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
  if (fuzzyLessEqual(LRL_H_INFINITY, delay))
    return LRL_S_INT_INF;
  Delay scaled_delay = std::stof(delayAsString(delay, sta_, 5));
  return round(scaled_delay * LRL_H_SCALING);
}
Delay SetupFixer::cost2Delay(Cost cost) const
{
  if (fuzzyLessEqual(LRL_S_INT_INF, cost))
    return LRL_H_INFINITY;
  return ((Delay) cost) / LRL_H_SCALING / LRL_H_STA_SCALING;
}

Index SetupFixer::getIndex(Vertex* vertex) const
{
  if (!vertex_index_map_.hasKey(vertex))
    return LRL_S_INVALIDE;
  return vertex_index_map_.at(vertex);
}

Delay SetupFixer::getDelay(Vertex* vertex)
{
  if (vertex_delay_map_.hasKey(vertex))
    return vertex_delay_map_.at(vertex).current_delay;
  Delay gate_delay = LRL_H_ZERO;
  PathRef path = sta_->vertexWorstSlackPath(vertex, max_);
  if (path.vertex(sta_) && vertex->isDriver(sta_->network())) {
    Pin* drvr_pin = vertex->pin();
    LibertyPort* drvr_port = sta_->network()->libertyPort(drvr_pin);
    const DcalcAnalysisPt* dcalc_ap = path.dcalcAnalysisPt(sta_);
    const int lib_ap = dcalc_ap->libertyIndex();
    const Delay load_cap = graph_delay_calc_->loadCap(drvr_pin, dcalc_ap);
    PathExpanded expanded(&path, sta_);
    int start_index = expanded.startIndex();
    int path_length = expanded.size();
    for (int i = start_index; i < path_length; i++) {
      Vertex* path_vertex = pathVertex(expanded, i);
      if (vertex == path_vertex) {
        LibertyPort* in_port = pathPort(expanded, i - 1);
        float prev_drive = LRL_H_ZERO;
        if (i >= 2) {
          LibertyPort* prev_drvr_port = pathPort(expanded, i - 2);
          if (prev_drvr_port) {
            prev_drive = prev_drvr_port->driveResistance();
          }
        }
        gate_delay = gateDelay(drvr_port, load_cap, dcalc_ap)
                     + prev_drive * in_port->cornerPort(lib_ap)->capacitance();
        vertex_delay_map_.insert(vertex, {gate_delay,  prev_drive, in_port});
        break;
      }
    }
  }
  return gate_delay;
}

Delay SetupFixer::getBound(Vertex* vertex, MinMax const* min_max)
{
  if (!vertex_bound_map_.hasKey(vertex)) {
    if (!vertex->isDriver(sta_->network()))
      return LRL_H_ZERO;
    Pin* drvr_pin = vertex->pin();
    LibertyPort* drvr_port = sta_->network()->libertyPort(drvr_pin);
    LibertyCell* cell = drvr_port->libertyCell();
    LibertyCellSeq* equiv_cells = sta_->equivCells(cell);
    if (!equiv_cells) {
      debugPrint(logger_, utl::LRL, "LRL", 3, "no equiv_cells for get bound");
      return getDelay(vertex);
    }
    PathRef path = sta_->vertexWorstSlackPath(vertex, max_);
    if (!path.vertex(sta_))
      return LRL_H_ZERO;
    Delay high_bound = LRL_H_ZERO;
    Delay low_bound = INF;
    const DcalcAnalysisPt* dcalc_ap = path.dcalcAnalysisPt(sta_);
    const char* drvr_port_name = drvr_port->name();
    const Delay load_cap = graph_delay_calc_->loadCap(drvr_pin, dcalc_ap);
    int lib_ap = dcalc_ap->libertyIndex();
    auto& vertex_delay = vertex_delay_map_.at(vertex);
    const float& prev_drive = vertex_delay.prev_drive;
    LibertyPort* in_port = vertex_delay.in_port;
    const char* in_port_name = in_port->name();
    for (LibertyCell* equiv : *equiv_cells) {
      LibertyCell* equiv_corner = equiv->cornerCell(lib_ap);
      LibertyPort* equiv_drvr = equiv_corner->findLibertyPort(drvr_port_name);
      LibertyPort* equiv_input = equiv_corner->findLibertyPort(in_port_name);
      Delay equiv_delay = gateDelay(equiv_drvr, load_cap, dcalc_ap)
                          + prev_drive * equiv_input->capacitance();
      if (fuzzyLess(equiv_delay, low_bound))
        low_bound = equiv_delay;
      if (fuzzyLess(high_bound, equiv_delay))
        high_bound = equiv_delay;
    }
    vertex_bound_map_.insert(vertex, {high_bound, low_bound});
    debugPrint(logger_,
              utl::LRL,
              "LRL",
              3,
              "{} high_bound = {}, low_bound = {}",
              vertex->name(sta_->sdcNetwork()),
              delayAsString(high_bound, sta_, 3),
              delayAsString(low_bound, sta_, 3));
  }
  auto& bound = vertex_bound_map_.at(vertex);
  if (min_max == max_)
    return bound.high_bound;
  return bound.low_bound;
}

Delay SetupFixer::getLowBound(Vertex* vertex)
{
  return getBound(vertex, MinMax::min());
}

Delay SetupFixer::getHighBound(Vertex* vertex)
{
  return getBound(vertex, MinMax::max());
}

Index SetupFixer::buildIndex(Vertex* vertex)
{
  if (!vertex_index_map_.hasKey(vertex))
    vertex_index_map_.insert(vertex, index_++);

  debugPrint(logger_,
             utl::LRL,
             "LRL",
             4,
             "{} , id = {}",
             vertex->name(sta_->sdcNetwork()),
             getIndex(vertex));
  return vertex_index_map_[vertex];
}

void SetupFixer::buildAllFaninEdge(const VertexSet& fanins)
{
  for (auto& vertex : fanins) {
    bool is_pi = true;
    buildIndex(vertex);
    SearchPredNonReg2 pred(sta_);
    bellman_max_iter_num_
        = std::max(vertex->level() / 20, bellman_max_iter_num_);
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
      addConstraintPrimeIn(vertex);
  }
}
void SetupFixer::buildALLEdgePrimeOut(const VertexSet& ends)
{
  for (auto& node : vertex_index_map_) {
    Vertex* vertex = node.first;
    if (ends.find(vertex) != ends.end())
      addConstraintPrimeOut(vertex);
  }
}

void SetupFixer::buildIndexMap(const VertexSet& ends)
{
  VertexSet all_ends;
  getAllRelatedEnds(ends, options_->ignoreClk(), all_ends);
  VertexSet fanins = findHoldFanins(all_ends);

  buildAllFaninEdge(all_ends);
  buildAllFaninEdge(fanins);
  buildALLEdgePrimeOut(all_ends);
}

void SetupFixer::addConstraintBase(Constraint && c)
{
  constraints_.emplace_back(c);
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             4,
             "cst: {} -> {} , cap = {}, cost = {}",
             c.start_node,
             c.end_node,
             isInf(c.capacity) ? "INF" : std::to_string(c.capacity),
             std::to_string(c.unit_cost));
}

Delay SetupFixer::costSlope(Vertex* pred, Vertex* succ)
{
  // return getCellDelay(succ);
  Vertex* vertex = succ;
  if (!vertex->isDriver(sta_->network()))
    return LRL_H_INFINITY;
  Pin* drvr_pin = vertex->pin();
  LibertyPort* drvr_port = sta_->network()->libertyPort(drvr_pin);
  LibertyCell* cell = drvr_port->libertyCell();
  LibertyCellSeq* equiv_cells = sta_->equivCells(cell);
  PathRef path = sta_->vertexWorstSlackPath(vertex, max_);
  if (!path.vertex(sta_))
    return LRL_H_INFINITY;
  const DcalcAnalysisPt* dcalc_ap = path.dcalcAnalysisPt(sta_);
  const char* drvr_port_name = drvr_port->name();
  const Delay load_cap = graph_delay_calc_->loadCap(drvr_pin, dcalc_ap);
  int lib_ap = dcalc_ap->libertyIndex();
  Delay min_delay = INF;
  Delay max_delay = INF;
  Delay min_area = INF;
  Delay max_area = INF;
  if (!equiv_cells) {
    debugPrint(logger_, utl::LRL, "LRL", 3, "no equiv_cells");
    return LRL_H_INFINITY;
  }
  for (LibertyCell* equiv : *equiv_cells) {
    LibertyCell* equiv_corner = equiv->cornerCell(lib_ap);
    LibertyPort* equiv_drvr = equiv_corner->findLibertyPort(drvr_port_name);
    Delay equiv_delay = gateDelay(equiv_drvr, load_cap, dcalc_ap);
    if ((fuzzyLess(equiv_delay, min_delay)) || fuzzyInf(min_delay)) {
      min_delay = equiv_delay;
      min_area = equiv->area();
    }
    if ((fuzzyLess(max_delay, equiv_delay)) || fuzzyInf(max_delay)) {
      max_delay = equiv_delay;
      max_area = equiv->area();
    }
  }
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             3,
             "{} : max area = {}, min area = {}",
             vertex->name(sta_->sdcNetwork()),
             delayAsString(max_area, sta_, 3),
             delayAsString(min_area, sta_, 3));
  Delay slope = cost2Delay(100)
                - (max_area - min_area) / (max_delay - min_delay)
                      / LRL_H_PRIORITY / LRL_H_STA_SCALING / LRL_H_SCALING;
  if (slope < LRL_H_ZERO)
    return LRL_H_ZERO;
  return slope;
}

Delay SetupFixer::getCellDelay(Vertex* vertex)
{
  Delay load_delay = LRL_H_ZERO;
  PathRef path = sta_->vertexWorstSlackPath(vertex, max_);
  if (!path.vertex(sta_))
    return LRL_H_ZERO;
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
    }
  }
  return load_delay;
}

void SetupFixer::addConstraintEdge(Vertex* pred, Vertex* succ)
{
  Delay delay = getCellDelay(succ);
  Delay gate_delay = getDelay(succ);
  // Delay delay = getDelay(succ);
  Delay diff = delay - gate_delay;
  Index pred_index = getIndex(pred);
  Index succ_index = getIndex(succ);
  Cost capacity = delay2Cost(costSlope(pred, succ));
  Delay low_bound = getLowBound(succ) + diff;
  // Delay low_bound = getLowBound(succ);
  // Delay high_bound = getHighBound(succ) + diff;
  addConstraintBase({pred_index, succ_index, capacity, -delay2Cost(delay)});
  addConstraintBase(
      {pred_index, succ_index, LRL_S_INT_INF, -delay2Cost(low_bound)});
  // addConstraintBase(
  //     succ_index, pred_index, LRL_S_INT_INF, delay2Cost(high_bound));
}

void SetupFixer::addConstraintPrimeOut(Vertex* po)
{
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  Index po_index = getIndex(po);
  Required req = sta_->vertexRequired(po, MinMax::max()) + slack_margin;
  addConstraintBase({po_index, T_INDEX, LRL_S_INT_INF, delay2Cost(req)});
  // solver_.SetNodeSupply(po_index, -LRL_S_INT_INF);
}

void SetupFixer::addConstraintPrimeIn(Vertex* pi)
{
  Index pi_index = getIndex(pi);
  Arrival pi_arrival = getArrival(pi, max_);
  addConstraintBase({S_INDEX, pi_index, LRL_S_INT_INF, -delay2Cost(pi_arrival)});
  pi_index_set_.insert(pi_index);
}

void SetupFixer::initAlgorithm()
{
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  VertexSet* ends = sta_->search()->endpoints();
  VertexSet failures;
  int cnt = 0;
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             2,
             "Violated Ends: (slack_margin = {})",
             delayAsString(slack_margin, sta_, 3));
  for (Vertex* vertex : *ends) {
    Slack slack = sta_->vertexSlack(vertex, MinMax::max());
    if (!fuzzyInf(slack) && fuzzyLess(slack, slack_margin)) {
      failures.insert(vertex);
      cnt++;
      debugPrint(logger_,
                 utl::LRL,
                 "LRL",
                 2,
                 "   {} : {}",
                 delayAsString(slack, sta_, 3),
                 vertex->name(sta_->sdcNetwork()));
    }
  }
  logger_->info(utl::LRL, 1303, "totally found {} vio ends", cnt);
  buildIndexMap(failures);
}

SetupResult SetupFixer::resolve()
{
  // Add each arc.
  for (auto& c : constraints_)
    solver_.AddArcWithCapacityAndUnitCost(
        c.start_node, c.end_node, c.capacity, c.unit_cost);

  // Add node supplies.
  solver_.SetNodeSupply(S_INDEX, LRL_S_INT_INF);
  solver_.SetNodeSupply(T_INDEX, -LRL_S_INT_INF);
  return solver_.SolveMaxFlowWithMinCost();
  // return solver_.Solve();
}

void SetupFixer::buildBellmanGraph(IdEdgeWeightSet& bellman_graph)
{
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
}

void SetupFixer::initArrivalMap()
{
  fixed_arrival_map_.clear();

  // the special node SRC
  fixed_arrival_map_.insert(S_INDEX, LRL_H_ZERO);
  // the special node SNK
  fixed_arrival_map_.insert(T_INDEX, LRL_H_ZERO);

  for (auto& node : vertex_index_map_) {
    const Index& index = node.second;
    fixed_arrival_map_.insert(index, LRL_H_INFINITY);
  }
}

bool SetupFixer::updateArrivalMap(const IdEdgeWeightSet& bellman_graph)
{
  bool changed = false;
  for (auto& edge : bellman_graph) {
    const Index& pred = edge.first.first;
    const Index& succ = edge.first.second;
    if (succ == S_INDEX || succ == T_INDEX)
      continue;
    const Delay& weight = edge.second;
    const Arrival& pred_arrival = fixed_arrival_map_[pred];
    const Arrival& succ_arrival = fixed_arrival_map_[succ];
    Arrival new_arrival = pred_arrival + weight;
    if (!(fuzzyEqual(LRL_H_INFINITY, pred_arrival) && !fuzzyZero(weight))
        && fuzzyLess(new_arrival, succ_arrival)) {
      fixed_arrival_map_[succ] = new_arrival;
      changed = true;
    }
  }
  return changed;
}

bool SetupFixer::remap()
{
  // Bellman Ford Shortest Path Algorithm
  bool changed = true;
  IdEdgeWeightSet bellman_graph;
  buildBellmanGraph(bellman_graph);
  initArrivalMap();
  for (size_t iter = 0; iter <= bellman_max_iter_num_ && changed; ++iter)
    changed = updateArrivalMap(bellman_graph);

  if (changed)
    logger_->warn(utl::LRL, 1200, "Bellman Graph infeasible");

  return !changed;
}

Delay SetupFixer::getNewDelay(Vertex* vertex)
{
  Delay shortest_delay = INF;
  if (vertex->isDriver(sta_->network())) {
    VertexInEdgeIterator edge_iter(vertex, sta_->graph());
    while (edge_iter.hasNext()) {
      Edge* edge = edge_iter.next();
      Vertex* fanin = edge->from(sta_->graph());
      VertexInEdgeIterator pred_edge_iter(fanin, sta_->graph());
      while (pred_edge_iter.hasNext()) {
        Edge* pred_edge = pred_edge_iter.next();
        Vertex* pred = pred_edge->from(sta_->graph());
        if (vertex_index_map_.hasKey(pred)) {
          Index pred_index = getIndex(pred);
          Index succ_index = getIndex(vertex);
          if (pred_index != S_INDEX) {
            Delay new_delay
                = fixed_arrival_map_[succ_index] - fixed_arrival_map_[pred_index];
            if (fuzzyLess(new_delay, shortest_delay))
              shortest_delay = new_delay;
          }
        }
      }
    }
    if (vertex_bound_map_.hasKey(vertex)) {
      Delay delay = getCellDelay(vertex);
      Delay gate_delay = getDelay(vertex);
      Delay diff = delay - gate_delay;
      shortest_delay -= diff;
      Delay low_bound = getLowBound(vertex);
      // Delay high_bound = getHighBound(vertex);
      Delay& high_bound = gate_delay;
      if (fuzzyLess(shortest_delay, low_bound + LRL_H_ROUNDING_LIMIT))
        shortest_delay = low_bound;
      else if (fuzzyLess(high_bound - LRL_H_ROUNDING_LIMIT, shortest_delay))
        shortest_delay = high_bound;
    }
  }
  return shortest_delay;
}

Delay SetupFixer::getResizeValue(Vertex* vertex)
{
  Delay new_delay = getNewDelay(vertex);
  if (fuzzyZero(new_delay) || fuzzyInf(new_delay))
    return LRL_H_ZERO;
  return new_delay - getDelay(vertex);
}

void SetupFixer::getResizeVertex(VertexSet& resize_vertex)
{
  for (auto& node : vertex_index_map_) {
    const Index& index = node.second;
    if (!pi_index_set_.hasKey(index)) {
      Vertex* vertex = node.first;
      Delay resize_value = getResizeValue(vertex);
      // if (delay2Cost(resize_value) != 0)
      if (fuzzyLessEqual(resize_value, LRL_H_ZERO - LRL_H_ROUNDING_LIMIT)
          || fuzzyLessEqual(LRL_H_ZERO + LRL_H_ROUNDING_LIMIT, resize_value))
        resize_vertex.insert(vertex);
    }
  }
}

LibertyCell* SetupFixer::findResizeCell(Vertex* vertex, Delay target_delay)
{
  Pin* drvr_pin = vertex->pin();
  LibertyPort* drvr_port = sta_->network()->libertyPort(drvr_pin);
  LibertyCell* cell = drvr_port->libertyCell();
  LibertyCellSeq* equiv_cells = sta_->equivCells(cell);
  PathRef path = sta_->vertexWorstSlackPath(vertex, max_);
  const DcalcAnalysisPt* dcalc_ap = path.dcalcAnalysisPt(sta_);
  const char* drvr_port_name = drvr_port->name();
  const Delay load_cap = graph_delay_calc_->loadCap(drvr_pin, dcalc_ap);
  int lib_ap = dcalc_ap->libertyIndex();
  Delay extreme_delay = LRL_H_ZERO;
  // Delay extreme_delay = LRL_H_INFINITY;
  LibertyCell* resize_cell = nullptr;
  auto& vertex_delay = vertex_delay_map_.at(vertex);
  const float& prev_drive = vertex_delay.prev_drive;
  LibertyPort* in_port = vertex_delay.in_port;
  const char* in_port_name = in_port->name();
  if (equiv_cells) {
    for (LibertyCell* equiv : *equiv_cells) {
      LibertyCell* equiv_corner = equiv->cornerCell(lib_ap);
      LibertyPort* equiv_drvr = equiv_corner->findLibertyPort(drvr_port_name);
      LibertyPort* equiv_input = equiv_corner->findLibertyPort(in_port_name);
      Delay equiv_delay = gateDelay(equiv_drvr, load_cap, dcalc_ap)
                          + prev_drive * equiv_input->capacitance();
      if (fuzzyLessEqual(equiv_delay, target_delay + LRL_H_ROUNDING_LIMIT)
          && fuzzyLess(extreme_delay, equiv_delay)) {
        // if (fuzzyLess(equiv_delay, extreme_delay)) {
        extreme_delay = equiv_delay;
        resize_cell = equiv;
      }
    }
  }
  return resize_cell;
}

void SetupFixer::getResizeQueue(sta::Map<Vertex*, LibertyCell*>& queue)
{
  VertexSet changed_vertex;
  getResizeVertex(changed_vertex);
  for (auto& vertex : changed_vertex) {
    Delay resize = getResizeValue(vertex);
    Delay new_delay = getNewDelay(vertex);
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               4,
               " {} : {} (new_delay = {})",
               vertex->name(sta_->sdcNetwork()),
               delayAsString(resize, sta_, 3),
               delayAsString(new_delay, sta_, 3));
    LibertyCell* resize_cell = findResizeCell(vertex, new_delay);
    if (resize_cell)
      queue.insert(vertex, resize_cell);
    else
      logger_->warn(utl::LRL,
                    1201,
                    "not found resize cell ({} -> {})",
                    vertex->name(sta_->sdcNetwork()),
                    delayAsString(new_delay, sta_, 3));
  }
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
               pred,
               succ,
               isInf(flow) ? "INF" : std::to_string(flow),
               isInf(cap) ? "INF" : std::to_string(cap),
               std::to_string(cost));
  }
}

}  // namespace lrl