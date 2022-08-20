#include "gui/gui.h"
#include "lrl/Laurel.hh"
#include "ord/OpenRoad.hh"

namespace lrl {

using namespace operations_research;
using namespace sta;

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
  makeVarAllModes();
}

void HoldFixer::makeVar(const Index& vertex_index)
{
  if (!arrival_.hasKey(sdc_))
    arrival_.insert(sdc_, std::make_shared<VertexMPVarMap>());
  const auto& a_var = arrival_.findKey(sdc_);
  if (a_var->find(vertex_index) == a_var->end())
    a_var->insert(vertex_index,
                  {solver_->MakeNumVar(  // max
                       LRL_H_ZERO,
                       LRL_H_INFINITY,
                       std::to_string(var_index_++)),
                   solver_->MakeNumVar(  // min
                       LRL_H_ZERO,
                       LRL_H_INFINITY,
                       std::to_string(var_index_++))});
}

void HoldFixer::makeWire(const Index& pred, const Index& succ)
{
  WirePair wire(pred, succ);
  if (!wire_.hasKey(wire))
    wire_.insert(wire,
                 solver_->MakeNumVar(
                     LRL_H_ZERO, LRL_H_INFINITY, std::to_string(var_index_++)));
  genWireConstraints(wire);
}

int HoldFixer::addConstraints(Delay low_bound, Delay high_bound)
{
  int index = ct_index_;
  ct_index_++;
  constraint_.insert(
      index,
      solver_->MakeRowConstraint(low_bound, high_bound, std::to_string(index)));
  return index;
}

void HoldFixer::makeVarFanins(const VertexSet& fanins, VertexIndexSet& pi_set)
{
  for (auto& vertex : fanins) {
    const Index& vertex_index = sdc_id_vertex_bmap_.right.at(vertex);
    makeVar(vertex_index);
    SearchPredNonReg2 pred(sta_);
    BfsBkwdIterator iter(BfsIndex::other, &pred, this);
    iter.enqueueAdjacentVertices(vertex);
    while (iter.hasNext()) {
      Vertex* fanin = iter.next();
      if (!fanin->isDriver(sta_->network()))
        iter.enqueueAdjacentVertices(fanin);
      else {
        const Index& fanin_index = sdc_id_vertex_bmap_.right.at(fanin);
        makeVar(fanin_index);
        makeWire(fanin_index, vertex_index);
        if (isRegister(fanin) || network_->isTopLevelPort(fanin->pin()))
          pi_set.insert(fanin_index);
      }
    }
  }
}

void HoldFixer::findHoldVioNameMM(VertexNameSet& vio_ends_name)
{
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  int mode_cnt = 0;
  int cnt = 0;
  sta_->forEachSdc([&]() {
    mode_cnt++;
    sdc_ = sta_->sdc();
    openroad_->getResizer()->estimateWireParasitics();
    sta_->findRequireds();
    VertexSet* ends = sta_->search()->endpoints();
    for (auto& vertex : *ends) {
      Slack slack = sta_->vertexSlack(vertex, MinMax::min());
      if (fuzzyLess(slack, slack_margin)) {
        vio_ends_name.insert(vertex->name(sta_->sdcNetwork()));
        cnt++;
      }
    }
  });
  logger_->info(utl::LRL,
                1100,
                "found {} vio ends in {} mode: (slack_margin = {}) ",
                cnt,
                mode_cnt,
                delayAsString(slack_margin, sta_, 3));
  for (auto& name : vio_ends_name)
    debugPrint(logger_, utl::LRL, "LRL", 4, "   {}", name);
}

bool HoldFixer::hasHoldViolation(Vertex* vertex,
                                 const VertexNameSet& vio_ends_name) const
{
  Name name = vertex->name(sta_->sdcNetwork());
  Slack slack = sta_->vertexSlack(vertex, MinMax::min());
  if (!fuzzyInf(slack) && vio_ends_name.find(name) != vio_ends_name.end())
    return true;
  return false;
}

void HoldFixer::getHoldViolations(const VertexNameSet& vio_ends_name,
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

void HoldFixer::makeRealVertexName()
{
  if (!sdc_id_vertex_bmap_.empty())
    sdc_id_vertex_bmap_.clear();

  VertexIterator vertex_iter(sta_->graph());
  while (vertex_iter.hasNext()) {
    Vertex* vertex = vertex_iter.next();
    Name vertex_name = vertex->name(sta_->sdcNetwork());
    if (vertex_name_id_map_.find(vertex_name) == vertex_name_id_map_.end()) {
      vertex_name_id_map_.insert({vertex_name, vertex_index_++});
      debugPrint(logger_,
                 utl::LRL,
                 "LRL",
                 4,
                 "Assign {} id = {}",
                 vertex_name.c_str(),
                 vertex_index_ - 1);
    }
    const Index& vertex_index = vertex_name_id_map_.at(vertex_name);
    sdc_id_vertex_bmap_.insert({vertex_index, vertex});
  }
}

Vertex* HoldFixer::findRealVertex(const Index& vertex_index) const
{
  const auto& find_iter = sdc_id_vertex_bmap_.left.find(vertex_index);
  if (find_iter != sdc_id_vertex_bmap_.left.end())
    return find_iter->second;
  else
    return nullptr;
}

void HoldFixer::makeVarAllModes()
{
  VertexNameSet vio_ends_name;
  findHoldVioNameMM(vio_ends_name);
  sta_->forEachSdc([&]() {
    sdc_ = sta_->sdc();
    openroad_->getResizer()->estimateWireParasitics();
    sta_->findRequireds();
    init();
    makeRealVertexName();
    VertexSet hold_failures_end;
    VertexSet hold_failures;
    getHoldViolations(vio_ends_name, hold_failures_end);
    getAllRelatedEnds(hold_failures_end, options_->ignoreClk(), hold_failures);
    if (!hold_failures.empty()) {
      VertexIndexSet pi_set;
      VertexSet fanins = findHoldFanins(hold_failures);
      makeVarFanins(fanins, pi_set);
      makeVarFanins(hold_failures, pi_set);
      genPrimeOutConstraints(hold_failures);
      genPrimeInConstraints(pi_set);
    }
  });
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
  Vertex* pred = sdc_id_vertex_bmap_.left.at(pred_index);
  Vertex* succ = sdc_id_vertex_bmap_.left.at(succ_index);
  Pin const* succ_pin = succ->pin();
  if (network_->isDriver(succ_pin) && !network_->isTopLevelPort(succ_pin)) {
    VertexPathIterator path_iter(succ, sta_);
    while (path_iter.hasNext()) {
      PathVertex* path = path_iter.next();
      PathExpanded expanded(path, sta_);
      const int start_index = expanded.startIndex();
      const int path_length = expanded.size();
      for (int i = start_index; i < path_length; i++) {
        Vertex* path_vertex = pathVertex(expanded, i);
        if (succ == path_vertex) {
          int pred_i = i - 2;
          if (pred != pathVertex(expanded, pred_i))
            ++pred_i;
          if (pred != pathVertex(expanded, pred_i))
            break;
          Delay load_delay = expanded.path(i)->arrival(sta_)
                             - expanded.path(pred_i)->arrival(sta_);
          if (fuzzyLess(max_delay, load_delay))
            max_delay = load_delay;
          if (fuzzyLess(load_delay, min_delay) || fuzzyZero(min_delay))
            min_delay = load_delay;
        }
      }
    }
  }
}

void HoldFixer::genPrimeInConstraints(const VertexIndexSet& pi_set)
{
  const auto& map = arrival_.findKey(sdc_);
  for (auto& vv : *map) {
    const Index& vertex_index = vv.first;
    if (pi_set.hasKey(vertex_index)) {
      Vertex* vertex = findRealVertex(vertex_index);
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

      MPVariable* a_max_var = vv.second.max;
      MPVariable* a_min_var = vv.second.min;
      // A(PI) == delay_init
      int index = addConstraints(arrival, arrival);
      constraint_.findKey(index)->SetCoefficient(a_max_var, 1);
      // a(PI)) == delay_init
      index = addConstraints(min_arrival, min_arrival);
      constraint_.findKey(index)->SetCoefficient(a_min_var, 1);
    }
  }
}

void HoldFixer::genPrimeOutConstraints(const VertexSet& ends)
{
  const auto& map = arrival_.findKey(sdc_);
  for (auto& vv : *map) {
    const Index& vertex_index = vv.first;
    Vertex* vertex = findRealVertex(vertex_index);
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

      MPVariable* a_max_var = vv.second.max;
      MPVariable* a_min_var = vv.second.min;
      // -A(PO) >= -R(PO)
      int index
          = addConstraints(-zoomIn(critical_long_arrival), LRL_H_INFINITY);
      constraint_.findKey(index)->SetCoefficient(a_max_var, -1);
      // a(PO)) >= r(PO)
      index = addConstraints(zoomIn(min_required), LRL_H_INFINITY);
      constraint_.findKey(index)->SetCoefficient(a_min_var, 1);
    }
  }
}

void HoldFixer::genWireConstraints(const WirePair& wire)
{
  const Index& pred = wire.first;
  const Index& succ = wire.second;
  const auto& a = arrival_.findKey(sta_->sdc());
  MPVariable* w_var = wire_.findKey(wire);
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
  int index = addConstraints(zoomIn(min_delay), LRL_H_INFINITY);
  constraint_.findKey(index)->SetCoefficient(a->at(pred).max, -1);
  constraint_.findKey(index)->SetCoefficient(a->at(succ).max, 1);
  constraint_.findKey(index)->SetCoefficient(w_var, -1);
  // aj <= ai + wij + delay  -->  ai - aj + wij >= -delay
  index = addConstraints(-zoomIn(max_delay), LRL_H_INFINITY);
  constraint_.findKey(index)->SetCoefficient(a->at(pred).min, 1);
  constraint_.findKey(index)->SetCoefficient(a->at(succ).min, -1);
  constraint_.findKey(index)->SetCoefficient(w_var, 1);
}

HoldResult HoldFixer::resolve()
{
  MPObjective* const objective = solver_->MutableObjective();
  for (auto& w : wire_) {
    Vertex* pred = findRealVertex(w.first.first);
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
  Delay padding = zoomOut(wire_.findKey(wire_pair)->solution_value());
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

// function copied and updated from OpenRoad/Resizer
// return inserted buffer fanout vertex
Vertex* HoldFixer::insertHoldDelay(Vertex* drvr,
                                   int buffer_count,
                                   PinSeq& load_pins,
                                   bool loads_have_out_port,
                                   LibertyCell* buffer_cell)
{
  Pin* drvr_pin = drvr->pin();
  Instance* parent = sta_->getDbNetwork()->topInstance();
  Net* drvr_net
      = sta_->network()->isTopLevelPort(drvr_pin)
            ? sta_->getDbNetwork()->net(sta_->getDbNetwork()->term(drvr_pin))
            : sta_->getDbNetwork()->net(drvr_pin);
  Net *in_net, *out_net;
  if (loads_have_out_port) {
    // Verilog uses nets as ports, so the net connected to an output port has
    // to be preserved.
    // Move the driver pin over to gensym'd net.
    in_net = makeUniqueNet();
    Port* drvr_port = sta_->network()->port(drvr_pin);
    Instance* drvr_inst = sta_->network()->instance(drvr_pin);
    sta_->disconnectPin(drvr_pin);
    sta_->connectPin(drvr_inst, drvr_port, in_net);
    out_net = drvr_net;
  } else {
    in_net = drvr_net;
    out_net = makeUniqueNet();
  }

  parasiticsInvalid(in_net);
  // Spread buffers between driver and load center.
  Point drvr_loc = sta_->getDbNetwork()->location(drvr_pin);
  Point load_center = findCenter(load_pins);
  int dx = (load_center.x() - drvr_loc.x()) / (buffer_count + 1);
  int dy = (load_center.y() - drvr_loc.y()) / (buffer_count + 1);

  Net* buf_in_net = in_net;
  Instance* buffer = nullptr;
  LibertyPort *input, *output;
  buffer_cell->bufferPorts(input, output);
  // drvr_pin->in_net->hold_buffer1->net2->hold_buffer2->out_net...->load_pins
  for (int i = 0; i < buffer_count; i++) {
    Net* buf_out_net = (i == buffer_count - 1) ? out_net : makeUniqueNet();
    // drvr_pin->drvr_net->hold_buffer->net2->load_pins
    string buffer_name = makeUniqueInstName("hold");
    buffer = sta_->getDbNetwork()->makeInstance(
        buffer_cell, buffer_name.c_str(), parent);
    journalMakeBuffer(buffer);
    inserted_buffer_count_++;
    designAreaIncr(area(sta_->getDbNetwork()->cell(buffer_cell)));

    sta_->connectPin(buffer, input, buf_in_net);
    sta_->connectPin(buffer, output, buf_out_net);
    Point buffer_loc(drvr_loc.x() + dx * i, drvr_loc.y() + dy * i);
    setLocation(buffer, buffer_loc);
    buf_in_net = buf_out_net;
    parasiticsInvalid(buf_out_net);
  }

  for (Pin* load_pin : load_pins) {
    Net* load_net = sta_->network()->isTopLevelPort(load_pin)
                        ? sta_->network()->net(sta_->network()->term(load_pin))
                        : sta_->network()->net(load_pin);
    if (load_net != out_net) {
      Instance* load = sta_->getDbNetwork()->instance(load_pin);
      Port* load_port = sta_->getDbNetwork()->port(load_pin);
      sta_->disconnectPin(load_pin);
      sta_->connectPin(load, load_port, out_net);
    }
  }
  // Pin* buffer_out_pin = sta_->network()->findPin(buffer, output);
  // resizeToTargetSlew(buffer_out_pin, false);
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
  Vertex* insert_from = sdc_id_vertex_bmap_.left.at(vertex_index);
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
      Vertex* fanout = sdc_id_vertex_bmap_.left.at(wire_pair.second);
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
