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

SetupFixer::HighLowBound& SetupFixer::getBound(VertexPair const& p)
{
  typedef HighLowBound::Bound Bound;

  auto it = bound_.find(p);
  if (it == bound_.end())
  {
    Vertex* v = p.second;

#define ALTER_RETURN(D) \
  return bound_.insert({p, {{D, INF}, {D, INF}}}).first->second;

    if (!v->isDriver(sta_->network()))
      ALTER_RETURN(LRL_H_ZERO);

    Pin* out_pin = v->pin();
    LibertyPort* out_port = sta_->network()->libertyPort(out_pin);
    LibertyCell* cell = out_port->libertyCell();
    LibertyCellSeq* equiv_cells = sta_->equivCells(cell);
    if (!equiv_cells) {
      debugPrint(logger_, utl::LRL, "LRL", 3, "no equiv_cells for get bound");
      Delay const& d = delays_.delay(p);
      ALTER_RETURN(d);
    }

    auto& factor = delays_.map.at(p);
    DcalcAnalysisPt const* dcalc_ap = factor.dcalc_ap;
    int const lib_ap = dcalc_ap->libertyIndex();
    LibertyPort* in_port = factor.in_port;
    const char* out_port_name = out_port->name();
    const char* in_port_name = in_port->name();
    Capacitance const load_cap = graph_delay_calc_->loadCap(out_pin, dcalc_ap);
    Resistance const& prev_drive = factor.prev_drive;

    Bound high = {-INF, INF};
    Bound low = {INF, INF};
    for (LibertyCell* equiv : *equiv_cells) {
      LibertyCell* equiv_corner = equiv->cornerCell(lib_ap);
      LibertyPort* equiv_out = equiv_corner->findLibertyPort(out_port_name);
      LibertyPort* equiv_input = equiv_corner->findLibertyPort(in_port_name);
      Delay equiv_delay = gateDelay(equiv_out, load_cap, dcalc_ap)
                          + prev_drive * equiv_input->capacitance();
      if (fuzzyLess(equiv_delay, low.delay)) {
        low.delay = equiv_delay;
        low.area = equiv->area();
      }
      if (fuzzyLess(high.delay, equiv_delay)) {
        high.delay = equiv_delay;
        high.area = equiv->area();
      }
    }
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               3,
               "{} high_bound = {} (area = {}), low_bound = {} (area = {})",
               v->name(sta_->sdcNetwork()),
               delayAsString(high.delay, sta_, 3),
               delayAsString(high.area, sta_, 3),
               delayAsString(low.delay, sta_, 3),
               delayAsString(low.area, sta_, 3));
    it = bound_.insert({p, {high, low}}).first;
  }
  return it->second;
}
#undef ALTER_RETURN

Delay SetupFixer::getLowBound(VertexPair const& p)
{
  return getBound(p).low.delay;
}

Delay SetupFixer::getHighBound(VertexPair const& p)
{
  return getBound(p).high.delay;
}

void SetupFixer::buildDelay(PathExpanded& expanded,
                            VertexPair const& edge,
                            int succ_id,
                            DcalcAnalysisPt* dcalc_ap)
{
  if (!edge.second->isDriver(sta_->network()))
    return;

  LibertyPort* drvr = pathPort(expanded, succ_id - 2);
  Resistance prev_drive = LRL_H_ZERO;
  if (drvr)
    prev_drive = drvr->driveResistance();

  Pin* out_pin = expanded.path(succ_id)->pin(sta_);
  LibertyPort* out = network_->libertyPort(out_pin);
  LibertyPort* in = pathPort(expanded, succ_id - 1);
  Index const lib_ap = dcalc_ap->libertyIndex();
  Capacitance const load_cap = graph_delay_calc_->loadCap(out_pin, dcalc_ap);
  Delay d = gateDelay(out, load_cap, dcalc_ap)
            + prev_drive * in->cornerPort(lib_ap)->capacitance();

  delays_.insert(edge, {d, prev_drive, in, dcalc_ap});
}

void SetupFixer::buildAllFaninEdge(VertexSet const& ends,
                                   VertexSet& pi,
                                   VertexPairSet& s)
{
  for (auto& end : ends) {
    bellman_max_iter_num_ = std::max(end->level() / 20, bellman_max_iter_num_);
    VertexPathIterator path_iter(end, sta_);
    while (path_iter.hasNext()) {
      PathVertex* path = path_iter.next();
      PathExpanded expanded(path, sta_);
      DcalcAnalysisPt* dcalc_ap = path->dcalcAnalysisPt(sta_);
      const int start_index = expanded.startIndex();
      const int path_end = expanded.size() - 1;
      for (int i = path_end; i > start_index; --i) {
        Vertex* succ = pathVertex(expanded, i);
        bool is_end = succ == end;
        if (is_end || succ->isDriver(sta_->network())) {
          const int p_i = is_end ? i - 1 : i - 2;
          Vertex* pred = pathVertex(expanded, p_i);
          VertexPair pair(pred, succ);
          buildDelay(expanded, pair, i, dcalc_ap);
          s.insert(pair);
          if (p_i == start_index)
            pi.insert(pred);
        }
      }
    }
  }
}

void SetupFixer::buildALLEdgePrimeOut(const VertexSet& ends)
{
  for (auto& node : index_.map) {
    Vertex* vertex = node.first;
    if (ends.find(vertex) != ends.end())
      addConstraintPrimeOut(vertex);
  }
}

void SetupFixer::buildIndexMap(const VertexSet& ends)
{
  VertexSet all_ends;
  getAllRelatedEnds(ends, options_->ignoreClk(), all_ends);
  // VertexSet fanins = findHoldFanins(all_ends);

  VertexPairSet s;
  VertexSet pi;
  buildAllFaninEdge(all_ends, pi, s);
  addAllConstraintsEdge(s);
  // buildAllFaninEdge(fanins);
  addAllConstraintsPrimeIn(pi);
  buildALLEdgePrimeOut(all_ends);
}

void SetupFixer::addConstraintBase(Constraint&& c)
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

Delay SetupFixer::costSlope(VertexPair const& p)
{
  auto& bound = getBound(p);
  if (bound.isInf())
    return LRL_H_INFINITY;

  auto& high = bound.high;
  auto& low = bound.low;
  float slope = - (high.area - low.area) / (high.delay - low.delay)
                      / LRL_H_PRIORITY / LRL_H_STA_SCALING / LRL_H_SCALING;
  if (slope < LRL_H_ZERO)
    return LRL_H_ZERO;
  return slope;
}

void SetupFixer::addConstraintEdge(VertexPair const& p)
{
  Delay delay = delays_.delay(p);
  Index pred_index = index_[p.first];
  Index succ_index = index_[p.second];
  Cost capacity = delay2Cost(costSlope(p));
  Delay low_bound = getLowBound(p);
  // Delay high_bound = getHighBound(p);
  debugPrint(logger_,
            utl::LRL,
            "LRL",
            2,
            "add cst {}({}) -> {}({})",
            p.first->name(sta_->sdcNetwork()),
            pred_index,
            p.second->name(sta_->sdcNetwork()),
            succ_index);
  addConstraintBase({pred_index, succ_index, capacity, -delay2Cost(delay)});  
  addConstraintBase(
      {pred_index, succ_index, LRL_S_INT_INF, -delay2Cost(low_bound)});
  // addConstraintBase(
  //     {succ_index, pred_index, LRL_S_INT_INF, delay2Cost(high_bound)});
}

void inline SetupFixer::addAllConstraintsEdge(VertexPairSet const s)
{
  for (auto& edge : s)
    addConstraintEdge(edge);
}

void SetupFixer::addConstraintPrimeOut(Vertex* po)
{
  Slack const slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  Index po_index = index_[po];
  Required req = sta_->vertexRequired(po, MinMax::max()) + slack_margin;
  addConstraintBase({po_index, T_INDEX, LRL_S_INT_INF, delay2Cost(req)});
  // solver_.SetNodeSupply(po_index, -LRL_S_INT_INF);
}

void SetupFixer::addConstraintPrimeIn(Vertex* pi)
{
  Index pi_index = index_[pi];
  PathRef path = sta_->vertexWorstSlackPath(pi, max_);
  Arrival pi_arrival
      = path.vertex(sta_) ? path.arrival(sta_) : getArrival(pi, max_);
  addConstraintBase(
      {S_INDEX, pi_index, LRL_S_INT_INF, -delay2Cost(pi_arrival)});
}

void inline SetupFixer::addAllConstraintsPrimeIn(VertexSet& pi)
{
  for (auto& v : pi)
    addConstraintPrimeIn(v);
}

bool SetupFixer::initAlgorithm(bool iterative)
{
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  VertexSet* ends = sta_->search()->endpoints();
  VertexSet failures;
  // if (iterative) {
  //   Slack worst_slack = LRL_H_INFINITY;
  //   Vertex* worst_end;
  //   for (Vertex* vertex : *ends) {
  //     Slack slack = sta_->vertexSlack(vertex, MinMax::max());
  //     if (fuzzyLess(slack, worst_slack)) {
  //       worst_slack = slack;
  //       worst_end = vertex;
  //     }
  //   }
  //   if (fuzzyLess(worst_slack, slack_margin - LRL_H_ROUNDING_LIMIT)){
  //     failures.insert(worst_end);
  //     logger_->info(utl::LRL,
  //                   1303,
  //                   "worst_end: {} (slack = {})",
  //                   worst_end->name(sta_->sdcNetwork()),
  //                   delayAsString(worst_slack, this, 3));
  //   }
  // } else {
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
  logger_->info(utl::LRL, 1304, "totally found {} vio ends", cnt);
  // }
  buildIndexMap(failures);
  return !failures.empty();
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
    if (flow < capacity || isInf(capacity))
      bellman_graph.insert(IdEdgeWeight(IdEdge(pred, succ), delay));
    // A(i) <= A(j) - w(i,j); f(i,j) > 0
    if (flow > LRL_S_ZERO)
      bellman_graph.insert(IdEdgeWeight(IdEdge(succ, pred), -delay));
  }
}

void SetupFixer::initArrivalMap()
{
  fixed_arrival_.resize(index_.size(), LRL_H_ZERO);
}

bool SetupFixer::updateArrivalMap(const IdEdgeWeightSet& bellman_graph)
{
  bool changed = false;
  for (auto& edge : bellman_graph) {
    Index const& pred = edge.first.first;
    Index const& succ = edge.first.second;
    if (succ == S_INDEX || succ == T_INDEX)
      continue;
    Delay const& weight = edge.second;
    Arrival const& pred_arrival = fixed_arrival_.at(pred);
    Arrival const& succ_arrival = fixed_arrival_.at(succ);
    Arrival new_arrival = pred_arrival + weight;
    if (fuzzyLess(succ_arrival, new_arrival)) {
      fixed_arrival_.at(succ) = new_arrival;
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

  for (size_t i = 0; i <= bellman_max_iter_num_ + 2 && changed; ++i) {
    changed = updateArrivalMap(bellman_graph);
  }

  if (changed)
    logger_->warn(utl::LRL, 1200, "Bellman Graph infeasible");

  return !changed;
}

Delay SetupFixer::getNewDelay(VertexPair const& p) const
{
  Index pred = index_.map.at(p.first);
  Index succ = index_.map.at(p.second);
  return fixed_arrival_.at(succ) - fixed_arrival_.at(pred);
}

void SetupFixer::getResizeVertex(ResizeMap& resize_vertex)
{
  for (auto& ele : delays_.map) {
    VertexPair const& edge = ele.first;
    Delay const& origin_delay = ele.second.delay;
    if (fuzzyZero(origin_delay) || !edge.second->isDriver(sta_->network()))
      continue;

    Delay const low_bound = getLowBound(edge);
    Delay new_delay = getNewDelay(edge);
    if (fuzzyLess(new_delay, low_bound))
      new_delay = low_bound;
    if (fuzzyLess(LRL_H_ROUNDING_LIMIT, fabs(new_delay - origin_delay))
        && !fuzzyInf(new_delay)) {
      Vertex* succ = ele.first.second;
      resize_vertex.insert(succ, {&ele.second, new_delay});
    }
  }
}

LibertyCell* SetupFixer::findResizeCell(Vertex* v, ResizeInfo const& i)
{
  Delay extreme_delay = LRL_H_ZERO;
  LibertyCell* resize_cell = nullptr;

  Pin* out_pin = v->pin();
  LibertyPort* out_port = sta_->network()->libertyPort(out_pin);
  LibertyCell* cell = out_port->libertyCell();
  LibertyCellSeq* equiv_cells = sta_->equivCells(cell);

  if (equiv_cells) {
    auto& factor = i.info;
    DcalcAnalysisPt const* dcalc_ap = factor->dcalc_ap;
    int const lib_ap = dcalc_ap->libertyIndex();
    LibertyPort* in_port = factor->in_port;
    Capacitance const load_cap = graph_delay_calc_->loadCap(out_pin, dcalc_ap);
    Resistance const& prev_drive = factor->prev_drive;
    const char* out_port_name = out_port->name();
    const char* in_port_name = in_port->name();
    for (LibertyCell* equiv : *equiv_cells) {
      LibertyCell* equiv_corner = equiv->cornerCell(lib_ap);
      LibertyPort* equiv_drvr = equiv_corner->findLibertyPort(out_port_name);
      LibertyPort* equiv_input = equiv_corner->findLibertyPort(in_port_name);
      Delay equiv_delay = gateDelay(equiv_drvr, load_cap, dcalc_ap)
                          + prev_drive * equiv_input->capacitance();
      if (fuzzyLessEqual(equiv_delay, i.new_delay + LRL_H_ROUNDING_LIMIT)
          && fuzzyLess(extreme_delay, equiv_delay)) {
        extreme_delay = equiv_delay;
        resize_cell = equiv;
      }
    }
  }
  return resize_cell;
}

void SetupFixer::getResizeQueue(sta::Map<Vertex*, LibertyCell*>& queue)
{
  ResizeMap changed_vertex;
  getResizeVertex(changed_vertex);
  for (auto& ele : changed_vertex.map) {
    Vertex* succ = ele.first;
    ResizeInfo const& info = ele.second;
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               4,
               " {} (new_delay = {})",
               succ->name(sta_->sdcNetwork()),
               delayAsString(info.new_delay, sta_, 3));
    LibertyCell* resize_cell = findResizeCell(succ, info);
    if (resize_cell)
      queue.insert(succ, resize_cell);
    else
      logger_->warn(utl::LRL,
                    1201,
                    "not found resize cell ({} -> {})",
                    succ->name(sta_->sdcNetwork()),
                    delayAsString(info.new_delay, sta_, 3));
  }
}

void SetupFixer::report() const
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