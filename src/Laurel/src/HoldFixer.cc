#include "gui/gui.h"
#include "lrl/Laurel.hh"
#include "ord/OpenRoad.hh"

namespace lrl {

using namespace operations_research;
using namespace sta;

#define addConstraints(l, h) \
  (solver_->MakeRowConstraint(l, h, std::to_string(ct_index_++)))

#define makeNumVar() \
  (solver_->MakeNumVar(LRL_H_ZERO, LRL_H_INFINITY, std::to_string(var_index_++)))

////////////////////////////////////////////////////////
//
//  class HoldFixer
//
////////////////////////////////////////////////////////

HoldFixer::HoldFixer(OpenRoad* openroad, Options* options, bool multi_mode)
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
  makeVarAllModes(multi_mode);
}

void HoldFixer::makeVar(const Index& vertex_index)
{
  if (!arrival_.hasKey(sdc_))
    arrival_.insert(sdc_, std::make_shared<VertexMPVarMap>());
  const auto& a_var = arrival_.findKey(sdc_);
  if (a_var->find(vertex_index) == a_var->end())
    a_var->insert({vertex_index, ArrivalVariable({makeNumVar(), makeNumVar()})});
}

void HoldFixer::makeWire(const Index& pred, const Index& succ)
{
  WirePair wire(pred, succ);
  if (wire_.find(wire) == wire_.end())
    wire_.insert({wire, makeNumVar()});
}

void HoldFixer::makeVarFanins(const VertexSet& ends,
                              VertexIndexSet& pi_set,
                              WirePairSet& w_set)
{
  for (auto& end : ends) {
    VertexPathIterator path_iter(end, sta_);
    while (path_iter.hasNext()) {
      PathVertex* path = path_iter.next();
      PathExpanded expanded(path, sta_);
      const int start_index = expanded.startIndex();
      const int path_length = expanded.size();
      for (int i = path_length-1; i > start_index; --i) {
        Vertex* succ = pathVertex(expanded, i);
        bool is_end = succ == end;
        if (is_end || succ->isDriver(sta_->network())) {
          const int p_i = is_end ? i-1:i-2;
          Vertex* pred = pathVertex(expanded, p_i);
          Index const& s_id = getIndex(succ);
          Index const& p_id = getIndex(pred);
          makeVar(s_id);
          makeVar(p_id);
          makeWire(p_id, s_id);
          w_set.insert(WirePair(p_id, s_id));
          Delay d = expanded.path(i)->arrival(sta_)
                            - expanded.path(p_i)->arrival(sta_);
          delays_.insert(p_id, s_id, d);
          if (isRegister(pred) || network_->isTopLevelPort(pred->pin()))
            pi_set.insert(p_id);
        }
      }
    }
  }
}

void HoldFixer::findHoldVioNameCore(NameSet& vio_ends_name)
{
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  VertexSet* ends = sta_->search()->endpoints();
  for (auto& vertex : *ends) {
    Slack slack = sta_->vertexSlack(vertex, MinMax::min());
    if (fuzzyLess(slack, slack_margin))
      vio_ends_name.insert(vertex->name(sta_->sdcNetwork()));
  }
}

void HoldFixer::findHoldVioNameMM(bool multi_mode, NameSet& vio_ends_name)
{
  int mode_cnt = 0;
  if (multi_mode) {
    sta_->forEachSdc([&]() {
      mode_cnt++;
      sdc_ = sta_->sdc();
      sta_->findRequireds();
      findHoldVioNameCore(vio_ends_name);
    });
  } else {
    mode_cnt++;
    findHoldVioNameCore(vio_ends_name);
  }
  logger_->info(utl::LRL,
                1100,
                "found {} vio ends in {} mode: (multi mode: {})",
                vio_ends_name.size(),
                mode_cnt,
                multi_mode ? "on":"off");
  for (auto& name : vio_ends_name)
    debugPrint(logger_, utl::LRL, "LRL", 4, "   {}", name);
}

bool HoldFixer::hasHoldViolation(Vertex* vertex,
                                 const NameSet& vio_ends_name) const
{
  Name name = vertex->name(sta_->sdcNetwork());
  Slack slack = sta_->vertexSlack(vertex, MinMax::min());
  if (!fuzzyInf(slack) && vio_ends_name.find(name) != vio_ends_name.end())
    return true;
  return false;
}

void HoldFixer::getHoldViolations(const NameSet& vio_ends_name,
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
  for (Vertex* vertex : *ends)
    if (hasHoldViolation(vertex, vio_ends_name))
      hold_failures.insert(vertex);
}

Index const& HoldFixer::getIndex(Vertex* v)
{
  if (!vertex_map_.hasKey(v)) {
    Name n = v->name(sta_->sdcNetwork());
    if (vertex_map_.insert(n, v))
      debugPrint(logger_,
                utl::LRL,
                "LRL",
                4,
                "Assign {} id = {}",
                n.c_str(),
                vertex_map_.at(v));
  }
  return vertex_map_.at(v);
}

void HoldFixer::findAllIndex()
{
  VertexIterator vertex_iter(sta_->graph());
  while (vertex_iter.hasNext()) {
    Vertex* v = vertex_iter.next();
    Pin* pin = v->pin();
    if (network_->isDriver(pin) 
        || network_->isTopLevelPort(pin) 
        || isRegister(v)) {
      Name n = v->name(sta_->sdcNetwork());
      if (vertex_map_.hasKey(n))
        vertex_map_.insert(n, v);
    }
  }
}

void HoldFixer::makeVarAllModesCore(NameSet const& vio_ends_name)
{
  init();
  delays_.clear();
  vertex_map_.clear();
  VertexSet hold_failures_end;
  VertexSet hold_failures;
  getHoldViolations(vio_ends_name, hold_failures_end);
  getAllRelatedEnds(hold_failures_end, options_->ignoreClk(), hold_failures);
  if (!hold_failures.empty()) {
    VertexIndexSet pi_set;
    WirePairSet w_set;
    // VertexSet fanins = findHoldFanins(hold_failures);
    // makeVarFanins(fanins, pi_set);
    makeVarFanins(hold_failures, pi_set, w_set);
    genAllWireConstraints(w_set);
    genPrimeOutConstraints(hold_failures);
    genPrimeInConstraints(pi_set);
  }
}

void HoldFixer::makeVarAllModes(bool multi_mode)
{
  NameSet vio_ends_name;
  findHoldVioNameMM(multi_mode, vio_ends_name);
  if (multi_mode) {
    sta_->forEachSdc([&]() {
      sdc_ = sta_->sdc();
      sta_->findRequireds();
      makeVarAllModesCore(vio_ends_name);
    });
    findAllIndex();
  } else {
    makeVarAllModesCore(vio_ends_name);
  }
}

Delay HoldFixer::getCriLongArrival(Vertex* end) const
{
  PathRef path = sta_->vertexWorstSlackPath(end, MinMax::max());
  return path.arrival(sta_);
}

void HoldFixer::getDelay(const Index& pred_index,
                         const Index& succ_index,
                         Delay& min_delay,
                         Delay& max_delay)
{
  Vertex* succ = vertex_map_.at(succ_index);
  Pin const* succ_pin = succ->pin();
  if (!network_->isDriver(succ_pin) || network_->isTopLevelPort(succ_pin))
    return;
  WirePair w(pred_index, succ_index);
  if(!delays_.hasKey(w)) {
    VertexPathIterator path_iter(succ, sta_);
    while (path_iter.hasNext()) {
      PathVertex* path = path_iter.next();
      PathExpanded expanded(path, sta_);
      const int start_index = expanded.startIndex();
      const int path_length = expanded.size();
      for (int i = start_index; i < path_length; i++) {
        Vertex* path_vertex = pathVertex(expanded, i);
        if (succ == path_vertex) {
          if (i-2 < start_index)
            break;
          Vertex* pred = pathVertex(expanded, i-2);
          if (!fuzzyInf(sta_->vertexSlack(pred, max_))) {
            Index const& p_id = getIndex(pred);
            Delay load_delay = expanded.path(i)->arrival(sta_)
                              - expanded.path(i-2)->arrival(sta_);
            delays_.insert(p_id, succ_index, load_delay);
            break;
          }
        }
      }
    }
  }
  if (delays_.hasKey(w)) {
    min_delay = delays_.min(w);
    max_delay = delays_.max(w);
  }
}

void HoldFixer::genPrimeInConstraints(const VertexIndexSet& pi_set)
{
  const auto& map = arrival_.findKey(sdc_);
  for (auto& vv : *map) {
    const Index& vertex_index = vv.first;
    if (pi_set.hasKey(vertex_index)) {
      Vertex* vertex = vertex_map_.at(vertex_index);
      PathRef path = sta_->vertexWorstSlackPath(vertex, max_);
      Delay arrival = path.vertex(sta_) ? zoomIn(path.arrival(sta_))
                                        : zoomIn(getArrival(vertex, max_));
      path = sta_->vertexWorstSlackPath(vertex, MinMax::min());
      Delay min_arrival = path.vertex(sta_)
                              ? zoomIn(path.arrival(sta_))
                              : zoomIn(getArrival(vertex, MinMax::min()));
      debugPrint(logger_,
                 utl::LRL,
                 "LRL",
                 3,
                 "PI {} , A = {}, a = {}",
                 vertex->name(sta_->sdcNetwork()),
                 delayAsString(zoomOut(arrival), sta_, 3),
                 delayAsString(zoomOut(min_arrival), sta_, 3));

      // A(PI) == delay_init
      MPConstraint* c_max = addConstraints(arrival, arrival);
      c_max->SetCoefficient(vv.second.max, 1);
      // a(PI)) == delay_init
      MPConstraint* c_min = addConstraints(min_arrival, min_arrival);
      c_min->SetCoefficient(vv.second.min, 1);
    }
  }
}

void HoldFixer::genPrimeOutConstraints(const VertexSet& ends)
{
  const auto& map = arrival_.findKey(sdc_);
  for (auto& vv : *map) {
    const Index& vertex_index = vv.first;
    Vertex* vertex = vertex_map_.at(vertex_index);
    if (ends.hasKey(vertex)) {
      Delay critical_long_arrival_value = getCriLongArrival(vertex);
      Delay min_required = sta_->vertexRequired(vertex, MinMax::min());
      Delay max_required_value = sta_->vertexRequired(vertex, MinMax::max())
                                 - std::max(LRL_H_ZERO, sdc_worst_slack_);
      Delay critical_long_arrival
          = std::max(critical_long_arrival_value, max_required_value);
      // what if whole path don't satisfy hold bound
      if (fuzzyLess(critical_long_arrival_value, min_required)
          || complete_vio_ends_.find(vertex_index)
                 != complete_vio_ends_.end()) {
        critical_long_arrival = sta_->vertexRequired(vertex, MinMax::max());
        complete_vio_ends_.insert(vertex_index);
      }

      if (fuzzyLess(sta_->vertexSlack(vertex, max_), LRL_H_ZERO))
        logger_->warn(utl::LRL,
                      1011,
                      "PO {} has setup violation",
                      vertex->name(sta_->sdcNetwork()));
      debugPrint(logger_,
                 utl::LRL,
                 "LRL",
                 3,
                 "PO {} , R = {}, r = {}",
                 vertex->name(sta_->sdcNetwork()),
                 delayAsString(critical_long_arrival, sta_, 3),
                 delayAsString(min_required, sta_, 3));

      // -A(PO) >= -R(PO)
      MPConstraint* c_max
          = addConstraints(-zoomIn(critical_long_arrival), LRL_H_INFINITY);
      c_max->SetCoefficient(vv.second.max, -1);
      // a(PO)) >= r(PO)
      MPConstraint* c_min
          = addConstraints(zoomIn(min_required), LRL_H_INFINITY);
      c_min->SetCoefficient(vv.second.min, 1);
    }
  }
}

void HoldFixer::genWireConstraints(WirePair const& wire)
{
  Index const& pred = wire.first;
  Index const& succ = wire.second;
  auto const& a = arrival_.findKey(sta_->sdc());
  MPVariable* w_var = wire_.at(wire);
  Delay min_delay = LRL_H_ZERO;
  Delay max_delay = LRL_H_ZERO;
  getDelay(pred, succ, min_delay, max_delay);
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             4,
             "delay between {} -> {} : min = {}, max = {}",
             pred,
             succ,
             delayAsString(min_delay, sta_, 3),
             delayAsString(max_delay, sta_, 3));
  // Aj >= Ai + wij + delay  -->  -Ai + Aj - wij >= delay
  MPConstraint* c_max = addConstraints(zoomIn(min_delay), LRL_H_INFINITY);
  c_max->SetCoefficient(a->at(pred).max, -1);
  c_max->SetCoefficient(a->at(succ).max, 1);
  c_max->SetCoefficient(w_var, -1);
  // aj <= ai + wij + delay  -->  ai - aj + wij >= -delay
  MPConstraint* c_min = addConstraints(-zoomIn(max_delay), LRL_H_INFINITY);
  c_min->SetCoefficient(a->at(pred).min, 1);
  c_min->SetCoefficient(a->at(succ).min, -1);
  c_min->SetCoefficient(w_var, 1);
}

void HoldFixer::genAllWireConstraints(WirePairSet const& w_set)
{
  for(auto& w : w_set)
    genWireConstraints(w);
}

HoldResult HoldFixer::resolve()
{
  MPObjective* const objective = solver_->MutableObjective();
  for (auto& w : wire_) {
    Vertex* pred = vertex_map_.at(w.first.first);
    int priority = LRL_H_PRIORITY + pred->level();
    objective->SetCoefficient(w.second, priority);
  }
  objective->SetMinimization();
  logger_->info(
      utl::LRL, 1103, "Number of variables = {}", solver_->NumVariables());
  logger_->info(
      utl::LRL, 1104, "Number of constraints = {}", solver_->NumConstraints());
  HoldResult result = solver_->Solve();
  logger_->info(utl::LRL,
                1105,
                "Objective value = {} (PRIORITY {})",
                objective->Value(),
                LRL_H_PRIORITY);
  return result;
}

Delay HoldFixer::getWirePadding(const WirePair& wire_pair) const
{
  Delay padding = zoomOut(wire_.at(wire_pair)->solution_value());
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             4,
             "padding between {} -> {} : {}",
             wire_pair.first,
             wire_pair.second,
             delayAsString(padding, sta_, 3));
  return padding;
}

void HoldFixer::getAllWirePairFrom(const Index& vertex_index,
                                   WirePairSet& wire_pair_set)
{
  for (auto& w : wire_) {
    const WirePair& wire_pair = w.first;
    if (vertex_index == wire_pair.first) {
      wire_pair_set.insert(wire_pair);
    }
  }
}

void HoldFixer::getNonZeroPaddingPred(VertexIndexSet& vio_pred_set)
{
  for (auto& w : wire_) {
    const WirePair& wire_pair = w.first;
    const Index& pred = wire_pair.first;
    Delay padding = zoomOut(w.second->solution_value());
    if (fuzzyLess(LRL_H_ROUNDING_LIMIT, padding)) {
      debugPrint(logger_,
                 utl::LRL,
                 "LRL",
                 4,
                 "non zero padding between {} -> {} : {}",
                 wire_pair.first,
                 wire_pair.second,
                 delayAsString(padding, sta_, 3));
      if (vio_pred_set.find(pred) == vio_pred_set.end())
        vio_pred_set.insert(pred);
    }
  }
}

////////////////////////////////////////////////////////
//
//  insert buffers
//
////////////////////////////////////////////////////////

void HoldFixer::sortHoldBuffers()
{
  findBuffers();
  if (!hold_buffers_.empty())
    hold_buffers_.clear();

  for (LibertyCell* buffer : buffer_cells_)
    hold_buffers_.push_back({buffer, bufferHoldDelay(buffer)});

  sort(hold_buffers_.begin(), hold_buffers_.end());
}

LibertyCell* HoldFixer::findHoldBuffer(Delay min_limit,
                                       int& count,
                                       Delay& real_delay)
{
  const Delay uncertainty = options_->uncertainty() / LRL_H_STA_SCALING;
  const Delay& min_buffer_delay
      = hold_buffers_[hold_buffers_.size() - 1].hold_delay_;
  LibertyCell* max_buffer = hold_buffers_[hold_buffers_.size() - 1].buffer_;
  Delay max_delay = min_buffer_delay;
  count = 1;
  real_delay = max_delay;

  if (!fuzzyLess(min_limit, min_buffer_delay)) {
    for (const BufferDelay& buffer_delay : hold_buffers_) {
      const Delay& buffer_min_delay = buffer_delay.hold_delay_;
      int min_count = std::ceil(min_limit / buffer_min_delay);
      Delay fixed_delay = buffer_min_delay * (Delay) (min_count);
      if (min_count > 1 && fuzzyLess(min_limit + uncertainty, fixed_delay))
        fixed_delay = buffer_min_delay * (Delay) (--min_count);
      if (fuzzyLess(fixed_delay + min_buffer_delay, min_limit + uncertainty)
          || fuzzyLess(std::abs(fixed_delay - min_limit), uncertainty)) {
        max_buffer = buffer_delay.buffer_;
        max_delay = buffer_min_delay;
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

#define pinNet(pin) \
  (sta_->network()->isTopLevelPort(pin) \
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

bool HoldFixer::insertBuffer(const Index& vertex_index,
                             Delay decrease,
                             WirePairSet& drvr_pair_set,
                             Vertex* drvr)
{
  PinSeq load_pins;
  Vertex* insert_from = vertex_map_.at(vertex_index);
  bool loads_have_out_port = false;
  Delay max_padding = LRL_H_ZERO;
  Delay min_padding = INF;
  if (drvr == nullptr)
    drvr = insert_from;
  if (drvr_pair_set.empty())
    getAllWirePairFrom(vertex_index, drvr_pair_set);

  for (auto& wire_pair : drvr_pair_set) {
    Delay padding = getWirePadding(wire_pair) - decrease;
    if (fuzzyLess(LRL_H_ROUNDING_LIMIT, padding)) {
      Vertex* fanout = vertex_map_.at(wire_pair.second);
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
          vertex_index, decrease + insert_delay, drvr_pair_set, buffer_fanout);
    return true;
  }
  return false;
}

bool HoldFixer::insertAllBuffer()
{
  bool ret = true;
  sortHoldBuffers();
  VertexIndexSet vio_pred_set;
  getNonZeroPaddingPred(vio_pred_set);
  for (auto& vertex_index : vio_pred_set) {
    WirePairSet drvr_pair_set;
    if (!insertBuffer(vertex_index, LRL_H_ZERO, drvr_pair_set))
      ret = false;
  }
  logger_->info(
      utl::LRL, 1110, "Totally inserted {} buffers", inserted_buffer_count_);
  return ret;
}

}  // namespace lrl
