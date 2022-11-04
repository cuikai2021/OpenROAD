#include "lrl/SetupFixer.hh"
#include "lrl/MCFSolver.hh"

namespace lrl {

using namespace operations_research;
using namespace sta;

////////////////////////////////////////////////////////
//
//  class SetupFixer : LaurelCollector
//
////////////////////////////////////////////////////////

Cost SetupFixer::delay2Cost(Delay delay)
{
  if (fuzzyLessEqual(LRL_H_INFINITY, delay))
    return MCFSolver::inf;
  Delay scaled_delay = std::stof(delayAsString(delay, sta_, 5));
  return round(scaled_delay * LRL_H_SCALING);
}

Delay SetupFixer::cost2Delay(Cost cost) const
{
  if (fuzzyLessEqual(MCFSolver::inf, cost))
    return LRL_H_INFINITY;
  return ((Delay) cost) / LRL_H_SCALING / LRL_H_STA_SCALING;
}

SetupFixer::HighLowBound& SetupFixer::getBound(Wire const& w)
{
  typedef HighLowBound::Bound Bound;

  auto it = wire_.map.find(w);
  assert(it != wire_.map.end());

  auto& bound = it->second.bound;
  if (bound.isDelayInf()) {
    Vertex* v = w.second;

    if (!v->isDriver(sta_->network()))
      return bound.setDelay(LRL_H_ZERO);

    Pin* out_pin = v->pin();
    LibertyPort* out_port = sta_->network()->libertyPort(out_pin);
    LibertyCell* cell = out_port->libertyCell();
    LibertyCellSeq* equiv_cells = sta_->equivCells(cell);
    if (!equiv_cells) {
      debugPrint(logger_, utl::LRL, "LRL", 3, "no equiv_cells for get bound");
      Delay const& d = wire_.delay(w);
      return bound.setDelay(d);
    }

    auto& factor = it->second.factor;
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
    bound = {high, low};
  }
  return bound;
}

Delay SetupFixer::getLowBound(Wire const& w)
{
  return getBound(w).low.delay;
}

Delay SetupFixer::getHighBound(Wire const& w)
{
  return getBound(w).high.delay;
}

void SetupFixer::buildDelay(Wire const& w, Edge* e)
{
  if (wire_.map.find(w) == wire_.map.end()) {
    wire_.insert(w, DelayFactor());
    if (!w.second->isDriver(sta_->network()))
      return;

    LibertyPort* drvr = network_->libertyPort(w.first->pin());
    Resistance prev_drive = LRL_H_ZERO;
    if (drvr)
      prev_drive = drvr->driveResistance();

    Pin* out_pin = e->to(graph_)->pin();
    Pin* in_pin = e->from(graph_)->pin();
    LibertyPort* out = network_->libertyPort(out_pin);
    LibertyPort* in = network_->libertyPort(in_pin);
    auto& aps = sta_->corners()->dcalcAnalysisPts();
    for (auto& dcalc_ap : aps) {
      Index const lib_ap = dcalc_ap->libertyIndex();
      Capacitance const load_cap = graph_delay_calc_->loadCap(out_pin, dcalc_ap);
      Delay d = gateDelay(out, load_cap, dcalc_ap)
                + prev_drive * in->cornerPort(lib_ap)->capacitance();

      wire_.insert(w, {d, prev_drive, in, dcalc_ap});
    }
  }
}

void SetupFixer::initWire(Vertex* pred,
                          Vertex* succ,
                          Edge* e)
{
  Wire wire(pred, succ);
  buildDelay(wire, e);
}

void SetupFixer::addALLConstraintsPrimeOut(const VertexSet& ends)
{
  index_graph_.forEach([&](Vertex* v, Index i){
    if (ends.find(v) != ends.end())
      addConstraintPrimeOut(v);
  });
}

void SetupFixer::buildIndexMap(const VertexSet& ends)
{
  VertexSet all_ends;
  getAllRelatedEnds(ends, options_->ignoreClk(), all_ends);

  for (auto& end : all_ends)
    bellman_max_iter_num_ = std::max(end->level() / 20, bellman_max_iter_num_);

  VertexSet pi;
  iterateWiresBfs(all_ends, pi);
  addAllConstraintsEdge();
  addAllConstraintsPrimeIn(pi);
  addALLConstraintsPrimeOut(all_ends);
}

Delay SetupFixer::costSlope(Wire const& w)
{
  auto& bound = getBound(w);
  if (bound.isAreaInf())
    return LRL_H_INFINITY;

  auto& high = bound.high;
  auto& low = bound.low;
  float slope = - (high.area - low.area) / (high.delay - low.delay)
                      / LRL_H_PRIORITY / LRL_H_STA_SCALING / LRL_H_SCALING;
  if (slope < LRL_H_ZERO)
    return LRL_H_ZERO;
  return slope;
}

void SetupFixer::addConstraintEdge(Wire const& w)
{
  Delay delay = wire_.delay(w);
  Index pred = index_graph_[w.first];
  Index succ = index_graph_[w.second];
  Cost capacity = delay2Cost(costSlope(w));
  Delay low_bound = getLowBound(w);
  // Delay high_bound = getHighBound(w);
  debugPrint(logger_,
            utl::LRL,
            "LRL",
            2,
            "add cst {}({}) -> {}({})",
            w.first->name(sta_->sdcNetwork()),
            pred,
            w.second->name(sta_->sdcNetwork()),
            succ);
  solver_.addConstraint({pred, succ, capacity, -delay2Cost(delay)});  
  solver_.addConstraint({pred, succ, MCFSolver::inf, -delay2Cost(low_bound)});
  // addConstraintBase(
  //     {succ_index, pred_index, MCFSolver::inf, delay2Cost(high_bound)});
}

void inline SetupFixer::addAllConstraintsEdge()
{
  for (auto& e : wire_.map)
    addConstraintEdge(e.first);
}

void SetupFixer::addConstraintPrimeOut(Vertex* po)
{
  Slack const slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  Index po_index = index_graph_[po];
  Required req = sta_->vertexRequired(po, MinMax::max()) + slack_margin;
  solver_.addConstraintSnk(po_index, delay2Cost(req));
}

void SetupFixer::addConstraintPrimeIn(Vertex* pi)
{
  Index pi_index = index_graph_[pi];
  PathRef path = sta_->vertexWorstSlackPath(pi, max_);
  Arrival a = path.vertex(sta_) ? path.arrival(sta_) : getArrival(pi, max_);
  solver_.addConstraintSrc(pi_index, -delay2Cost(a));
}

void inline SetupFixer::addAllConstraintsPrimeIn(VertexSet const& pi)
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

void SetupFixer::buildBellmanGraph()
{
  solver_.forEachArc([&](auto& c, Cost flow){
    Cost const& capacity = c.capacity;
    Index pred = c.start_node;
    Index succ = c.end_node;
    Delay delay = -cost2Delay(c.unit_cost);

    // build Bellman Graph with KKT condition for edge (i,j)
    // A(i) + w(i,j) >= A(j); f(i,j) < c(i,j)
    if (flow < capacity || isInf(capacity))
      index_graph_.addWeight({{pred, succ}, delay});
    // A(i) <= A(j) - w(i,j); f(i,j) > 0
    if (flow > MCFSolver::zero)
      index_graph_.addWeight({{succ, pred}, -delay});
  });
}

bool SetupFixer::remap()
{
  // remap by using bellmen graph
  buildBellmanGraph();
  bool ret = index_graph_.solve(LRL_H_ZERO, bellman_max_iter_num_ + 2);
  if (!ret)
    logger_->warn(utl::LRL, 1200, "Bellman Graph infeasible");

  index_graph_.clearGraph();
  return ret;
}

Delay SetupFixer::getNewDelay(Wire const& w) const
{
  return index_graph_.result(w.first) - index_graph_.result(w.second);
}

void SetupFixer::getResizeVertex(ResizeMap& resize_vertex)
{
  for (auto& ele : wire_.map) {
    Wire const& edge = ele.first;
    DelayFactor& factor = ele.second.factor;
    Delay const& origin_delay = factor.delay;
    if (fuzzyZero(origin_delay) || !edge.second->isDriver(sta_->network()))
      continue;

    Delay const low_bound = getLowBound(edge);
    Delay new_delay = getNewDelay(edge);
    if (fuzzyLess(new_delay, low_bound))
      new_delay = low_bound;
    if (fuzzyLess(LRL_H_ROUNDING_LIMIT, fabs(new_delay - origin_delay))
        && !fuzzyInf(new_delay)) {
      Vertex* succ = ele.first.second;
      resize_vertex.insert(succ, {&factor, new_delay});
    }
  }
}

void SetupFixer::report() const
{
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             3,
             "Minimum cost flow: {}",
             solver_.optimalCost());
  solver_.forEachArc([&](auto& c, Cost flow) {
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               3,
               "{} -> {}, flow = {}/{}, cost = {}",
               c.start_node,
               c.end_node,
               isInf(flow) ? "INF" : std::to_string(flow),
               isInf(c.capacity) ? "INF" : std::to_string(c.capacity),
               c.unit_cost);
  });
}

}  // namespace lrl