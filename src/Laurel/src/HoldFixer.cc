#include "gui/gui.h"
#include "lrl/Laurel.hh"
#include "ord/OpenRoad.hh"

namespace lrl {

using namespace operations_research;
using namespace sta;

////////////////////////////////////////////////////////
//
//  class HoldFixerTaskManager
//
////////////////////////////////////////////////////////

HoldFixerTaskManager::HoldFixerTaskManager(OpenRoad* openroad, Options* options)
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
  init();
}

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
//  class HoldFixer
//
////////////////////////////////////////////////////////

HoldFixer::HoldFixer(OpenRoad* openroad, Options* options)
    : LaurelAdapter(),
      solver_(MPSolver::CreateSolver(LRL_H_SOLVER)),
      options_(options)
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
  // makeVarAllModes(multi_mode);
}

#define makeNumVar()    \
  (solver_->MakeNumVar( \
      LRL_H_ZERO, LRL_H_INFINITY, ""))

void HoldFixer::makeVar(Vertex* v, ArrivalType const& t)
{
  auto& a_var = arrival_[sdc_][v];
  if (a_var.find(t) == a_var.end())
    a_var.insert({t, {makeNumVar(), makeNumVar()}});
}

void HoldFixer::registerWire(Wire const& w)
{
  if (wire_.find(w) == wire_.end())
    wire_.insert({w, {makeNumVar()}});
}

#define pathArrival(i) (expanded.path(i)->arrival(sta_))

void HoldFixer::makeVarFanins(VertexSet const& ends,
                              VertexSet& pi_set,
                              WireTypeMap& w)
{
  for (auto& end : ends) {
    VertexPathIterator path_iter(end, sta_);
    while (path_iter.hasNext()) {
      PathVertex* path = path_iter.next();
      PathExpanded expanded(path, sta_);
      const int start_index = expanded.startIndex();
      const int path_end = expanded.size() - 1;
      
      auto const wire_type = (path->clkEdge(sta_)->transition() == RiseFall::fall())
                                ? ArrivalType::HALF_AHEAD
                                : ArrivalType::DEFAULT;

      for (int i = path_end; i > start_index; --i) {
        Vertex* succ = pathVertex(expanded, i);
        bool is_end = succ == end;
        if (is_end || succ->isDriver(sta_->network())) {
          const int p_i = is_end ? i - 1 : i - 2;
          Vertex* pred = pathVertex(expanded, p_i);
          makeVar(succ, wire_type);
          makeVar(pred, wire_type);
          Wire wire(pred, succ);
          registerWire(wire);
          w[wire].insert(wire_type);
          // w.insert(wire, wire_type);
          Delay d = pathArrival(i) - pathArrival(p_i);
          delays_.insert(wire, d);
          if (isRegister(pred) || network_->isTopLevelPort(pred->pin()))
            pi_set.insert(pred);
        }
      }
    }
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
  Vertex* worst_vertex;
  sta_->worstSlack(max_, sdc_worst_slack_, worst_vertex);
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             4,
             "WNS end {}, slack = {}",
             worst_vertex->name(sta_->network()),
             delayAsString(sdc_worst_slack_, sta_, 3));
  for (Vertex* v : *ends)
    if (hasHoldViolation(v, vio_ends))
      hold_failures.insert(v);
}

void HoldFixer::makeVarAllModesCore(VertexSet const& hold_failures_end)
{
  init();
  delays_.map.clear();
  VertexSet hold_failures;
  getAllRelatedEnds(hold_failures_end, options_->ignoreClk(), hold_failures);
  if (!hold_failures.empty()) {
    VertexSet pi_set;
    WireTypeMap w;
    makeVarFanins(hold_failures, pi_set, w);
    genAllWireConstraints(w);
    genPrimeOutConstraints(hold_failures);
    genPrimeInConstraints(pi_set);
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
                         Delay& min_delay,
                         Delay& max_delay)
{
  Pin const* succ_pin = w.second->pin();
  if (!network_->isDriver(succ_pin) || network_->isTopLevelPort(succ_pin))
    return;
  auto const& m = delays_.map;
  auto it = m.find(w);
  if (it != m.end()) {
    min_delay = it->second.min;
    max_delay = it->second.max;
  }
}

#define addConstraints(l, h) \
  (solver_->MakeRowConstraint(l, h, ""))

#define checkPathArrival(path, v, min_max)        \
  (path.vertex(sta_) ? zoomIn(path.arrival(sta_)) \
                     : zoomIn(getArrival(v, min_max)))

void HoldFixer::genPrimeInConstraints(VertexSet const& pi_set)
{
  const auto& m = arrival_[sdc_];
  for (auto& vv : m.map) {
    Vertex* v = vv.first;
    if (pi_set.hasKey(v)) {
      PathRef p = sta_->vertexWorstSlackPath(v, max_);
      Delay arrival = checkPathArrival(p, v, max_);
      p = sta_->vertexWorstSlackPath(v, MinMax::min());
      Delay min_arrival = checkPathArrival(p, v, MinMax::min());

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
          MPConstraint* c_max = addConstraints(arrival, arrival);
          c_max->SetCoefficient(unit.max, 1);
          // a(PI)) == delay_init
          MPConstraint* c_min = addConstraints(min_arrival, min_arrival);
          c_min->SetCoefficient(unit.min, 1);
        }
      }
    }
  }
}

#define checkPathRequired(path, v, min_max)        \
  (path.vertex(sta_) ? path.required(sta_) \
                     : sta_->vertexRequired(v, min_max))

void HoldFixer::genPrimeOutConstraints(const VertexSet& ends)
{
  const auto& m = arrival_[sdc_];
  for (auto& vv : m.map) {
    Vertex* v = vv.first;
    if (ends.hasKey(v)) {
      PathRef p = sta_->vertexWorstSlackPath(v, max_);
      Delay const max_path_req = checkPathRequired(p, v, max_);
      p = sta_->vertexWorstSlackPath(v, MinMax::min());
      Delay const min_path_req = checkPathRequired(p, v, MinMax::min());
      Delay const period = p.clock(sta_)->period();

      Delay const critical_long_arrival_value = getCriLongArrival(v);
      // Delay max_required_value
      //     = max_path_req - std::max(LRL_H_ZERO, sdc_worst_slack_);
      Delay critical_long_arrival
          = std::max(critical_long_arrival_value, max_path_req);
          // = std::max(critical_long_arrival_value, max_required_value);
      // what if whole path don't satisfy hold bound
      // if (fuzzyLess(critical_long_arrival_value, min_path_req)
      //     || complete_vio_ends_.find(v) != complete_vio_ends_.end()) {
      //   critical_long_arrival = max_path_req;
      //   complete_vio_ends_.insert(v);
      // }

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
          MPConstraint* c_max
              = addConstraints(-zoomIn(max_required), LRL_H_INFINITY);
          c_max->SetCoefficient(unit.max, -1);
          // a(PO)) >= r(PO)
          MPConstraint* c_min
              = addConstraints(zoomIn(min_required), LRL_H_INFINITY);
          c_min->SetCoefficient(unit.min, 1);
        }
      }
    }
  }
}

void HoldFixer::genWireConstraints(Wire const& wire, ArrivalType const& t)
{
  Vertex* pred = wire.first;
  Vertex* succ = wire.second;
  MPVariable* w_var = wire_[wire];
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
  // Aj >= Ai + wij + delay  -->  -Ai + Aj - wij >= delay
  MPConstraint* c_max = addConstraints(zoomIn(max_delay), LRL_H_INFINITY);
  c_max->SetCoefficient(a_pred.max, -1);
  c_max->SetCoefficient(a_succ.max, 1);
  c_max->SetCoefficient(w_var, -1);
  // aj <= ai + wij + delay  -->  ai - aj + wij >= -delay
  MPConstraint* c_min = addConstraints(-zoomIn(min_delay), LRL_H_INFINITY);
  c_min->SetCoefficient(a_pred.min, 1);
  c_min->SetCoefficient(a_succ.min, -1);
  c_min->SetCoefficient(w_var, 1);
}

void HoldFixer::genAllWireConstraints(WireTypeMap const& w)
{
  for (auto& ele : w.map) {
    auto const& types = *ele.second;
    Wire const& wire = ele.first;
    for (auto& t : types)
      genWireConstraints(wire, t);
  }
}

HoldResult HoldFixer::resolve()
{
  MPObjective* const objective = solver_->MutableObjective();
  for (auto& w : wire_) {
    Vertex* pred = w.first.first;
    int priority = LRL_H_PRIORITY + pred->level();
    objective->SetCoefficient(w.second, priority);
  }
  objective->SetMinimization();
  HoldResult result = solver_->Solve();
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             1,
             "Objective value = {} (PRIORITY {})",
             objective->Value(),
             LRL_H_PRIORITY);
  return result;
}

Delay HoldFixer::getWirePadding(Wire const& w) const
{
  Delay padding = zoomOut(wire_.at(w)->solution_value());
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
    Delay padding = zoomOut(w.second->solution_value());
    if (fuzzyLess(LRL_H_ROUNDING_LIMIT, padding))
      vio_pred_set.insert(pred);
  }
}

////////////////////////////////////////////////////////
//
//  insert buffers
//
////////////////////////////////////////////////////////

void HoldFixer::sortHoldBuffers()
{
  if (!hold_buffers_)
    hold_buffers_ = std::make_shared<BufferSeq>().get();
  if (hold_buffers_->empty()) {
    findBuffers();
    for (LibertyCell* buffer : buffer_cells_)
      hold_buffers_->push_back({buffer, bufferHoldDelay(buffer)});

    sort(hold_buffers_->begin(), hold_buffers_->end());
  }
}

LibertyCell* HoldFixer::findHoldBuffer(Delay min_limit,
                                       int& count,
                                       Delay& real_delay)
{
  Delay const uncertainty = options_->uncertainty() / LRL_H_STA_SCALING;
  Delay const upper_limit = min_limit + uncertainty;
  Delay const& min_delay = hold_buffers_->back().hold_delay;
  LibertyCell* max_buffer = hold_buffers_->back().buffer;
  Delay max_delay = min_delay;
  count = 1;
  real_delay = max_delay;

  if (!fuzzyLess(min_limit, min_delay)) {
    for (auto& h : *hold_buffers_) {
      Delay const& d = h.hold_delay;
      int min_count = std::ceil(min_limit / d);
      Delay fixed_delay = d * (Delay) (min_count);
      if (min_count > 1 && fuzzyLess(upper_limit, fixed_delay))
        fixed_delay = d * (Delay) (--min_count);

      if (fuzzyLess(fixed_delay + min_delay, upper_limit)
          || fuzzyLess(std::abs(fixed_delay - min_limit), uncertainty)) {
        max_buffer = h.buffer;
        max_delay = d;
        real_delay = fixed_delay;
        count = min_count;
        break;
      }
    }
  }

  if (max_buffer)
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               4,
               "choose buffer {}, delay {}, fixed delay {}",
               max_buffer->name(),
               delayAsString(max_delay, this, 3),
               delayAsString(real_delay, this, 3));
  return max_buffer;
}

#define pinNet(pin)                                       \
  (sta_->network()->isTopLevelPort(pin)                   \
       ? sta_->network()->net(sta_->network()->term(pin)) \
       : sta_->network()->net(pin))

Vertex* HoldFixer::insertHoldDelay(Vertex* drvr,
                                   int buffer_count,
                                   PinSeq& load_pins,
                                   bool loads_have_out_port,
                                   LibertyCell* buffer_cell)
{
  Pin* drvr_pin = drvr->pin();
  Net* drvr_net = pinNet(drvr_pin);
  Net *in_net, *out_net;
  if (loads_have_out_port) {
    // Verilog uses nets as ports, so the net connected to an output port has
    // to be preserved.
    // Move the driver pin over to gensym'd net.
    in_net = makeUniqueNet();
    connectNetLoad(drvr_pin, in_net);
    out_net = drvr_net;
  } else {
    in_net = drvr_net;
    out_net = makeUniqueNet();
  }
  parasiticsInvalid(in_net);

  Point drvr_loc = sta_->getDbNetwork()->location(drvr_pin);
  Point load_center = findCenter(load_pins);
  int dx = (load_center.x() - drvr_loc.x()) / (buffer_count + 1);
  int dy = (load_center.y() - drvr_loc.y()) / (buffer_count + 1);

  Net* buf_in_net = in_net;
  Instance* buffer = nullptr;
  LibertyPort *input, *output;
  buffer_cell->bufferPorts(input, output);
  for (int i = 0; i < buffer_count; i++) {
    Net* buf_out_net = (i == buffer_count - 1) ? out_net : makeUniqueNet();
    Point buffer_loc(drvr_loc.x() + dx * i, drvr_loc.y() + dy * i);
    makeBufferInstance("hold", buffer_loc, buffer_cell, buffer);

    sta_->connectPin(buffer, input, buf_in_net);
    sta_->connectPin(buffer, output, buf_out_net);
    buf_in_net = buf_out_net;
    parasiticsInvalid(buf_out_net);
  }

  for (Pin* load_pin : load_pins) {
    Net* load_net = pinNet(load_pin);
    if (load_net != out_net) {
      connectNetLoad(load_pin, out_net);
    }
  }

  Pin* out_pin = db_network_->findPin(buffer, output);
  return sta_->graph()->pinLoadVertex(out_pin);
}

Pin* HoldFixer::findFanoutPin(Vertex* pred, Vertex* succ) const
{
  if (!succ->isDriver(sta_->network()))
    return succ->pin();
  VertexOutEdgeIterator pred_edge_iter(pred, sta_->graph());
  while (pred_edge_iter.hasNext()) {
    Edge* pred_edge = pred_edge_iter.next();
    Vertex* fanout = pred_edge->to(sta_->graph());
    VertexInEdgeIterator succ_edge_iter(succ, sta_->graph());
    while (succ_edge_iter.hasNext()) {
      Edge* succ_edge = succ_edge_iter.next();
      Vertex* fanin = succ_edge->from(sta_->graph());
      if (fanin == fanout)
        return fanin->pin();
    }
  }
  return nullptr;
}

bool HoldFixer::insertBuffer(Vertex* insert_from,
                             Delay decrease,
                             WireSet& drvr_pair_set,
                             Vertex* drvr)
{
  PinSeq load_pins;
  bool loads_have_out_port = false;
  Delay max_padding = LRL_H_ZERO;
  Delay min_padding = INF;
  if (drvr == nullptr)
    drvr = insert_from;
  if (drvr_pair_set.empty())
    getAllWireFrom(insert_from, drvr_pair_set);

  for (auto& wire_pair : drvr_pair_set) {
    Delay padding = getWirePadding(wire_pair) - decrease;
    if (fuzzyLess(LRL_H_ROUNDING_LIMIT, padding)) {
      Vertex* fanout = wire_pair.second;
      Pin* fanout_pin = findFanoutPin(drvr, fanout);
      if (fanout_pin != nullptr) {
        load_pins.push_back(fanout_pin);
        if (sta_->network()->direction(fanout_pin)->isAnyOutput()
            && sta_->network()->isTopLevelPort(fanout_pin))
          loads_have_out_port = true;

        if (fuzzyLess(max_padding, padding))
          max_padding = padding;
        if (fuzzyLess(padding, min_padding) || fuzzyInf(min_padding))
          min_padding = padding;
      }
    }
  }

  const Delay uncertainty = options_->uncertainty() / LRL_H_STA_SCALING;
  Delay padding = max_padding;
  if (!fuzzyLess(max_padding - min_padding, uncertainty)) {
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               1,
               "padding diff in all {} fanout from {} higher then uncertainty",
               load_pins.size(),
               insert_from->name(sta_->sdcNetwork()));
    padding = min_padding;
  }

  int buffer_count = 1;
  Delay insert_delay = LRL_H_ZERO;
  LibertyCell* buffer_cell
      = findHoldBuffer(padding, buffer_count, insert_delay);
  if (buffer_cell != nullptr && !load_pins.empty()) {
    Vertex* buffer_fanout = insertHoldDelay(
        drvr, buffer_count, load_pins, loads_have_out_port, buffer_cell);
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               4,
               "inserted {} buffer, buffer fanout vertex = {}, decrease = {}, "
               "max_padding = {}",
               std::to_string(buffer_count),
               buffer_fanout->name(sta_->sdcNetwork()),
               delayAsString(insert_delay, this, 3),
               delayAsString(max_padding, this, 3));
    if (fuzzyLessEqual(insert_delay, max_padding))
      return insertBuffer(
          insert_from, decrease + insert_delay, drvr_pair_set, buffer_fanout);
    return true;
  }
  return false;
}

bool HoldFixer::insertAllBuffer()
{
  bool ret = true;
  sortHoldBuffers();
  VertexSet vio_pred_set;
  getNonZeroPaddingPred(vio_pred_set);
  for (auto& v : vio_pred_set) {
    WireSet drvr_pair_set;
    if (!insertBuffer(v, LRL_H_ZERO, drvr_pair_set))
      ret = false;
  }
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             1,
             "inserted {} buffers",
             inserted_buffer_count_);
  return ret;
}

}  // namespace lrl
