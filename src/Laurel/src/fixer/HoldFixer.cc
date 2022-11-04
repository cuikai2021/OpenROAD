#include "lrl/HoldFixer.hh"

namespace lrl {

using namespace sta;

////////////////////////////////////////////////////////
//
//  class HoldFixerTaskManager
//
////////////////////////////////////////////////////////

void HoldFixerTaskManager::ExclusiveSet::expand(VertexSet const& related,
                                                std::vector<Vertex*>& queue)
{
  auto& set = seq.back();
  for (auto& v : related)
    if(set->insert(v).second)
      queue.push_back(v);
}

void HoldFixerTaskManager::getAllRelatedFailures(Vertex* v,
                                                 VSet const& failures,
                                                 VertexSet& results)
{
  results.clear();
  VertexSet related;
  results.insert(v);
  getAllRelatedEnds(results, options_->ignoreClk(), related);
  for (auto& r : related)
    if(failures.find(r) != failures.end())
      results.insert(r);
}

void HoldFixerTaskManager::findTaskBfs(VSet& failures)
{
  while(!failures.empty()) {
    std::vector<Vertex*> queue;
    tasks_.seq.push_back(std::make_shared<VertexSet>());
    Vertex* f = *(failures.begin());
    tasks_.seq.back()->insert(f);
    queue.push_back(f);
    while(!queue.empty()) {
      VertexSet related;
      Vertex* q = queue.back();
      queue.pop_back();
      failures.erase(q);
      getAllRelatedFailures(q, failures, related);
      tasks_.expand(related, queue);
    }
  }
}

void HoldFixerTaskManager::findTaskCore()
{
  const Slack margin = options_->slackMargin() / LRL_H_STA_SCALING;
  VertexSet* ends = sta_->search()->endpoints();
  VSet failures;
  for (auto& e : *ends)
    if (fuzzyLess(sta_->vertexSlack(e, MinMax::min()), margin))
      failures.insert(e);
  
  findTaskBfs(failures);
}

void HoldFixerTaskManager::findTaskCoreMM()
{
  logger_->warn(utl::LRL,
                1012,
                "Laurel doesn't support Partial fix in Multi-Mode, try "
                "\"laurel -hold -global -multi_mode\"");
}

void HoldFixerTaskManager::findTasks(bool multi_mode)
{
  if (multi_mode)
    findTaskCoreMM();
  else
    findTaskCore();
}

////////////////////////////////////////////////////////
//
//  class HoldFixer : public LaurelCollector
//
////////////////////////////////////////////////////////

#define MAKE_UNIT()    \
  (solver_->makeUnit())

void HoldFixer::makeVar(Vertex* v, ArrivalType const& t)
{
  auto& a_var = arrival_[sdc_][v];
  if (a_var.find(t) == a_var.end())
    a_var.insert({t, {MAKE_UNIT(), MAKE_UNIT()}});
}

void HoldFixer::registerWire(Wire const& w, Edge* e)
{
  if (wire_.find(w) == wire_.end())
    wire_.insert({w, WireInfo(MAKE_UNIT(), e)});
}

void HoldFixer::initWire(Vertex* pred,
                         Vertex* succ,
                         Edge* e)
{
  Wire wire(pred, succ);
  registerWire(wire, e);
  VertexPathIterator path_iter(pred, sta_);
  while (path_iter.hasNext()) {
    PathVertex* path = path_iter.next();
    auto const type = (path->clkEdge(sta_)->transition() == RiseFall::fall())
                          ? ArrivalType::HALF_AHEAD
                          : ArrivalType::DEFAULT;
    makeVar(pred, type);
    makeVar(succ, type);
    wire_.at(wire).types->insert(type);
  }
}

void HoldFixer::findHoldVioCore(VertexSet& vio_ends)
{
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  VertexSet* ends = sta_->search()->endpoints();
  for (auto& e : *ends) {
    Slack slack = sta_->vertexSlack(e, MinMax::min());
    if (fuzzyLess(slack, slack_margin))
      vio_ends.insert(e);
  }
}

void HoldFixer::findHoldVioMM(bool multi_mode, VertexSet& vio_ends)
{
  int mode_cnt = 0;
  if (multi_mode) {
    sta_->forEachSdc([&]() {
      mode_cnt++;
      sdc_ = sta_->sdc();
      sta_->findRequireds();
      findHoldVioCore(vio_ends);
    });
  } else {
    mode_cnt++;
    findHoldVioCore(vio_ends);
  }
  logger_->info(utl::LRL,
                1100,
                "found {} vio ends in {} mode: (multi mode: {})",
                vio_ends.size(),
                mode_cnt,
                multi_mode ? "on" : "off");
  for (auto& v : vio_ends)
    debugPrint(
        logger_, utl::LRL, "LRL", 4, "   {}", v->name(sta_->sdcNetwork()));
}

bool HoldFixer::hasHoldViolation(Vertex* v, VertexSet const& vio_ends) const
{
  Slack slack = sta_->vertexSlack(v, MinMax::min());
  return !fuzzyInf(slack) && vio_ends.hasKey(v);
}

void HoldFixer::getHoldViolations(VertexSet const& vio_ends,
                                  VertexSet& hold_failures)
{
  VertexSet* ends = sta_->search()->endpoints();
  for (Vertex* v : *ends)
    if (hasHoldViolation(v, vio_ends))
      hold_failures.insert(v);
}

void HoldFixer::makeVarAllModesCore(VertexSet const& hold_failures_end)
{
  init();
  VertexSet hold_failures;
  getAllRelatedEnds(hold_failures_end, options_->ignoreClk(), hold_failures);
  if (!hold_failures.empty()) {
    VertexSet pi_set;
    iterateWiresBfs(hold_failures, pi_set);
    genAllWireConstraints();
    genPrimeOutConstraints(hold_failures);
    genPrimeInConstraints(pi_set);
    setAllPriority();
  }
}

void HoldFixer::buildFailureEnds(bool is_partial,
                                 VertexSet* v,
                                 VertexSet const& vio_ends)
{
  VertexSet ends;
  if (is_partial)
    getHoldViolations(vio_ends, ends);
  else
    ends.insert(v->begin(), v->end());
  makeVarAllModesCore(ends);
}

void HoldFixer::initAlgorithm(bool multi_mode, VertexSet* v)
{
  const bool partial = !v;
  VertexSet vio_ends;
  if (partial)
    findHoldVioMM(multi_mode, vio_ends);

  if (multi_mode) {
    sta_->forEachSdc([&]() {
      sdc_ = sta_->sdc();
      sta_->findRequireds();
      buildFailureEnds(partial, v, vio_ends);
    });
  } else
    buildFailureEnds(partial, v, vio_ends);
}

Delay HoldFixer::getCriLongArrival(Vertex* end) const
{
  PathRef path = sta_->vertexWorstSlackPath(end, MinMax::max());
  return path.arrival(sta_);
}

void HoldFixer::getDelay(Wire const& w,
                         Delay& min,
                         Delay& max)
{
  Pin const* succ_pin = w.second->pin();
  if (!network_->isDriver(succ_pin) || network_->isTopLevelPort(succ_pin))
    return;

  auto it = wire_.find(w);
  if(it != wire_.end()) {
    auto& info = it->second;
    auto& delay = info.delay;
    if (delay.isInf()) {
      Edge* e = info.edge;
      TimingArcSetArcIterator arc_iter(e->timingArcSet());
      auto& aps = sta_->corners()->dcalcAnalysisPts();
      while(arc_iter.hasNext()) {
        TimingArc* arc = arc_iter.next();
        for (auto& a : aps) {
          Delay d = graph_->arcDelay(e, arc, a->index());
          delay.store(d);
        }
      }
    }
    min = delay.min;
    max = delay.max;
  }

}

#define ADD_CONSTRAINTS(l, h) \
  (solver_->MakeRowConstraint(l, h, ""))

#define CHECK_PATH_ARRIVAL(path, v, min_max)        \
  (path.vertex(sta_) ? zoomIn(path.arrival(sta_)) \
                     : zoomIn(getArrival(v, min_max)))

void HoldFixer::genPrimeInConstraints(VertexSet const& pi_set)
{
  const auto& m = arrival_[sdc_];
  for (auto& vv : m.map) {
    Vertex* v = vv.first;
    if (pi_set.hasKey(v)) {
      PathRef p = sta_->vertexWorstSlackPath(v, max_);
      Delay arrival = CHECK_PATH_ARRIVAL(p, v, max_);
      p = sta_->vertexWorstSlackPath(v, MinMax::min());
      Delay min_arrival = CHECK_PATH_ARRIVAL(p, v, MinMax::min());

      auto const& a_vars = vv.second;
      for (int i = ArrivalType::DEFAULT; i<ArrivalType::SIZE; ++i) {
        auto it = a_vars->find((ArrivalType)i);
        if (it != a_vars->end()) {
          debugPrint(logger_,
                    utl::LRL,
                    "LRL",
                    3,
                    "PI {}, TypeId = {}, A = {}, a = {}",
                    v->name(sta_->sdcNetwork()),
                    i,
                    delayAsString(zoomOut(arrival), sta_, 3),
                    delayAsString(zoomOut(min_arrival), sta_, 3));
          auto const& unit = it->second;
          // A(PI) == delay_init
          solver_->setEqual(unit.max, arrival);
          // a(PI)) == delay_init
          solver_->setEqual(unit.min, min_arrival);
        }
      }
    }
  }
}

#define CHECK_PATH_REQUIRED(path, v, min_max)        \
  (path.vertex(sta_) ? path.required(sta_) \
                     : sta_->vertexRequired(v, min_max))

void HoldFixer::genPrimeOutConstraints(const VertexSet& ends)
{
  const auto& m = arrival_[sdc_];
  for (auto& vv : m.map) {
    Vertex* v = vv.first;
    if (ends.hasKey(v)) {
      PathRef p = sta_->vertexWorstSlackPath(v, max_);
      Delay const max_path_req = CHECK_PATH_REQUIRED(p, v, max_);
      p = sta_->vertexWorstSlackPath(v, MinMax::min());
      Delay const min_path_req = CHECK_PATH_REQUIRED(p, v, MinMax::min());
      Delay const period = p.clock(sta_)->period();

      Delay const critical_long_arrival_value = getCriLongArrival(v);
      Delay critical_long_arrival
          = std::max(critical_long_arrival_value, max_path_req);

      if (fuzzyLess(sta_->vertexSlack(v, max_), LRL_H_ZERO))
        logger_->warn(utl::LRL,
                      1011,
                      "PO {} has setup violation",
                      v->name(sta_->sdcNetwork()));

      auto const& a_vars = vv.second;
      bool const is_half_ahead = fuzzyLess(max_path_req, 0.75 * period);
      for (int i = ArrivalType::DEFAULT; i<ArrivalType::SIZE; ++i) {
        auto it = a_vars->find((ArrivalType)i);
        if (it != a_vars->end()) {
          Delay max_required = critical_long_arrival;
          Delay min_required = min_path_req;
          if(is_half_ahead)
            switch(i) {
              case ArrivalType::HALF_AHEAD:
                max_required = max_path_req + period; break;
              case ArrivalType::DEFAULT:
                min_required = min_path_req - 0.5*period; break;
            }
          debugPrint(logger_,
                    utl::LRL,
                    "LRL",
                    3,
                    "PO {}, TypeId = {}, R = {}, r = {}",
                    v->name(sta_->sdcNetwork()),
                    i,
                    delayAsString(max_required, sta_, 3),
                    delayAsString(min_required, sta_, 3));
          auto const& unit = it->second;
          // -A(PO) >= -R(PO)
          solver_->setLess(unit.max, zoomIn(max_required));
          // a(PO)) >= r(PO)
          solver_->setGreater(unit.min, zoomIn(min_required));
        }
      }
    }
  }
}

void HoldFixer::genWireConstraints(Wire const& wire, ArrivalType const& t)
{
  Vertex* pred = wire.first;
  Vertex* succ = wire.second;
  MPVariable* w_var = wire_.at(wire).var;
  Delay min_delay = LRL_H_ZERO;
  Delay max_delay = LRL_H_ZERO;
  getDelay(wire, min_delay, max_delay);
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             4,
             "delay between {} -> {} (TypeId {}): min = {}, max = {}",
             pred->name(sta_->sdcNetwork()),
             succ->name(sta_->sdcNetwork()),
             t,
             delayAsString(min_delay, sta_, 3),
             delayAsString(max_delay, sta_, 3));

  auto const& a_pred = arrival_[sdc_][pred][t];
  auto const& a_succ = arrival_[sdc_][succ][t];

  typedef LPSolver::UnitCoeff UC;
  int const len = 3;
  // Aj >= Ai + wij + delay  -->  -Ai + Aj - wij >= delay
  UC max_vars[len] = {{a_pred.max, -1}, {a_succ.max, 1}, {w_var, -1}};
  solver_->setPolynomialGreater(max_vars, len, zoomIn(max_delay));
  // aj <= ai + wij + delay  -->  ai - aj + wij >= -delay
  UC min_vars[len] = {{a_pred.min, 1}, {a_succ.min, -1}, {w_var, 1}};
  solver_->setPolynomialGreater(min_vars, len, -zoomIn(min_delay));
}

void HoldFixer::genAllWireConstraints()
{
  for (auto& ele : wire_) {
    auto const& types = *(ele.second.types);
    Wire const& wire = ele.first;
    for (auto& t : types)
      genWireConstraints(wire, t);
  }
  wire_.clear_types();
}

void HoldFixer::setAllPriority()
{
  for (auto& w : wire_) {
    Vertex* pred = w.first.first;
    int priority = LRL_H_PRIORITY + pred->level();
    solver_->setPriority(w.second.var, priority);
  }
}

Delay HoldFixer::getWirePadding(Wire const& w) const
{
  Delay padding = zoomOut(solver_->solutionValue(wire_.at(w).var));
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             4,
             "padding between {} -> {} : {}",
             w.first->name(sta_->sdcNetwork()),
             w.second->name(sta_->sdcNetwork()),
             delayAsString(padding, sta_, 3));
  return padding;
}

void HoldFixer::getAllWireFrom(Vertex* v, WireSet& wire_set)
{
  for (auto& w : wire_) {
    Wire const& wire_pair = w.first;
    if (v == wire_pair.first) {
      wire_set.insert(wire_pair);
    }
  }
}

void HoldFixer::getNonZeroPaddingPred(VertexSet& vio_pred_set)
{
  for (auto& w : wire_) {
    const Wire& wire_pair = w.first;
    Vertex* pred = wire_pair.first;
    Delay padding = zoomOut(w.second.var->solution_value());
    if (fuzzyLess(LRL_H_ROUNDING_LIMIT, padding))
      vio_pred_set.insert(pred);
  }
}

}  // namespace lrl
