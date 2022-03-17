#include "rsz/LPResizer.hh"

#include "gui/gui.h"
#include "ord/OpenRoad.hh"

namespace rsz {

using namespace operations_research;
using namespace sta;

////////////////////////////////////////////////////////
//
//  class LPHoldFixer
//
////////////////////////////////////////////////////////

LPHoldFixer::LPHoldFixer(OpenRoad* openroad, LPOptions* options)
    : Resizer(), solver_(MPSolver::CreateSolver(LP_SOLVER)), options_(options)
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

LPHoldFixer::~LPHoldFixer()
{
  for (auto A : arrival_max_)
    delete A.second;
  for (auto a : arrival_min_)
    delete a.second;
}

void LPHoldFixer::makeVar(std::string vertex_name, MMVertexMPVarMap& arrival)
{
  if (!arrival.hasKey(sdc_))
    arrival.insert(sdc_, new VertexMPVarMap());
  if (!arrival.findKey(sdc_)->hasKey(vertex_name))
    arrival.findKey(sdc_)->insert(
        vertex_name,
        solver_->MakeNumVar(
            LP_ZERO, LP_INFINITY, std::to_string(var_index_++)));
}

void LPHoldFixer::makeWire(std::string pred, std::string succ)
{
  WirePair wire(pred, succ);
  if (!wire_.hasKey(wire))
    wire_.insert(wire,
                 solver_->MakeNumVar(
                     LP_ZERO, LP_INFINITY, std::to_string(var_index_++)));
  genWireConstraints(wire);
}

int LPHoldFixer::addConstraints(float low_bound, float high_bound)
{
  int index = ct_index_;
  ct_index_++;
  constraint_.insert(
      index,
      solver_->MakeRowConstraint(low_bound, high_bound, std::to_string(index)));
  return index;
}

void LPHoldFixer::makeVarFanins(VertexSeq fanins)
{
  for (auto vertex : fanins) {
    std::string vertex_name = vertex->name(sta_->sdcNetwork());
    makeVar(vertex_name, arrival_max_);
    makeVar(vertex_name, arrival_min_);
    makeDelay(vertex);
    SearchPredNonReg2 pred(sta_);
    BfsBkwdIterator iter(BfsIndex::other, &pred, this);
    iter.enqueueAdjacentVertices(vertex);
    while (iter.hasNext()) {
      Vertex* fanin = iter.next();
      if (!fanin->isDriver(sta_->network()))
        iter.enqueueAdjacentVertices(fanin);
      else {
        std::string fanin_name = fanin->name(sta_->sdcNetwork());
        makeVar(fanin_name, arrival_max_);
        makeVar(fanin_name, arrival_min_);
        makeDelay(fanin);
        makeWire(fanin_name, vertex_name);
      }
    }
  }
}

void LPHoldFixer::findHoldVioNameMM()
{
  const Slack slack_margin = options_->slackMargin() / LP_STA_SCALING;
  sta_->forEachSdc([&]() {
    sdc_ = sta_->sdc();
    sta_->findRequireds();
    VertexSet* ends = sta_->search()->endpoints();
    for (auto vertex : *ends) {
      Slack slack = sta_->vertexSlack(vertex, MinMax::min());
      if (fuzzyLess(slack, slack_margin))
        vio_ends_name_.insert(vertex->name(sta_->sdcNetwork()));
    }
  });
  logger_->info(utl::RSZ,
                1100,
                "vio ends: (slack_margin = {}) ",
                delayAsString(slack_margin, sta_, 3));
  for (auto name : vio_ends_name_)
    logger_->info(utl::RSZ, 1101, "   {}", name);
}

bool LPHoldFixer::hasHoldViolation(Vertex* vertex) const
{
  std::string name = vertex->name(sta_->sdcNetwork());
  Slack slack = sta_->vertexSlack(vertex, MinMax::min());
  if (!fuzzyInf(slack) && vio_ends_name_.find(name) != vio_ends_name_.end())
    return true;
  return false;
}

VertexSet LPHoldFixer::getHoldViolations()
{
  VertexSet* ends = sta_->search()->endpoints();
  VertexSet hold_failures;
  Vertex* worst_vertex;
  sta_->worstSlack(max_, sdc_worst_slack_, worst_vertex);
  debugPrint(logger_,
             utl::RSZ,
             "LPHoldFixer",
             4,
             "WNS end {}, slack = {}",
             worst_vertex->name(sta_->network()),
             delayAsString(sdc_worst_slack_, sta_, 3));
  for (Vertex* vertex : *ends)
    if (hasHoldViolation(vertex))
      hold_failures.insert(vertex);
  return hold_failures;
}

void LPHoldFixer::makeVarAllModes()
{
  findHoldVioNameMM();
  sta_->forEachSdc([&]() {
    sdc_ = sta_->sdc();
    sta_->findRequireds();
    VertexSet hold_failures = getHoldViolations();
    hold_failures = getAllRelatedEnds(hold_failures, options_->ignoreClk());
    if (!hold_failures.empty()) {
      VertexSet fanins = findHoldFanins(hold_failures);
      VertexSeq sorted_fanins = sortHoldFanins(fanins);
      VertexSeq sorted_hold_failures = sortHoldFanins(hold_failures);
      makeVarFanins(sorted_fanins);
      makeVarFanins(sorted_hold_failures);
      genPOConstraints(&hold_failures);
      genPIConstraints();
    }
  });
}

bool LPHoldFixer::isPI(std::string vertex_name) const
{
  bool ret = false;
  for (auto w : wire_) {
    if (!vertex_name.compare(w.first.second))
      return false;
    else if (!vertex_name.compare(w.first.first))
      ret = true;
  }
  return ret;
}

VertexNameSeq LPHoldFixer::getSortedNameSeq(VertexMPVarMap* mp_var_map)
{
  VertexNameSeq name_seq;
  for (auto pair : *mp_var_map) {
    name_seq.push_back(pair.first);
  }
  std::sort(name_seq.begin(), name_seq.end());
  return name_seq;
}

float LPHoldFixer::getArrivalMax(Vertex* vertex) const
{
  float max_arrival = LP_ZERO;
  VertexPathIterator path_iter(vertex, sta_);
  while (path_iter.hasNext()) {
    Path* path = path_iter.next();
    float arrival = path->arrival(sta_);
    if ((fuzzyLess(max_arrival, arrival)) && !fuzzyInf(arrival))
      max_arrival = arrival;
  }
  return max_arrival;
}

float LPHoldFixer::getCriLongArrival(Vertex* end) const
{
  float sum = LP_ZERO;
  PathRef path = sta_->vertexWorstSlackPath(end, MinMax::max());
  PathExpanded expanded(&path, sta_);
  int start_index = expanded.startIndex();
  int path_length = expanded.size();
  for (int i = start_index; i < path_length; i++) {
    PathRef* path = expanded.path(i);
    Vertex* vertex = path->vertex(sta_);
    std::string vertex_name = vertex->name(sta_->sdcNetwork());
    if (isPI(vertex_name))
      sum += getArrivalMax(vertex);
    else
      sum += getDelay(vertex->name(sta_->sdcNetwork()));
  }
  return sum;
}

void LPHoldFixer::makeDelay(Vertex* vertex)
{
  std::string vertex_name = vertex->name(sta_->sdcNetwork());
  if (!delay_map_.hasKey(vertex_name)) {
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
    delay_map_.insert(vertex_name, load_delay);
  }
}

float LPHoldFixer::getDelay(std::string vertex_name) const
{
  if (!delay_map_.hasKey(vertex_name))
    return LP_ZERO;
  return delay_map_.at(vertex_name);
}

void LPHoldFixer::genPIConstraints()
{
  VertexMPVarMap* map = arrival_max_.findKey(sdc_);
  VertexNameSeq sorted_vertex = getSortedNameSeq(map);
  for (auto vertex_name : sorted_vertex) {
    if (isPI(vertex_name)) {
      Vertex* vertex = findRealVertex(vertex_name);
      float arrival = zoomIn(getArrivalMax(vertex));
      // float delay = zoomIn(getDelay(vertex_name));
      debugPrint(logger_,
                 utl::RSZ,
                 "LPHoldFixer",
                 3,
                 "PI {} , A = {}, a = {}",
                 vertex->name(sta_->sdcNetwork()),
                 delayAsString(zoomOut(arrival), sta_, 3),
                 delayAsString(zoomOut(arrival), sta_, 3));
      // A(PI) == delay_init
      int index = addConstraints(arrival, arrival);
      constraint_.findKey(index)->SetCoefficient(map->findKey(vertex_name), 1);
      // a(PI)) == delay_init
      index = addConstraints(arrival, arrival);
      constraint_.findKey(index)->SetCoefficient(
          arrival_min_.findKey(sdc_)->findKey(vertex_name), 1);
    }
  }
}

void LPHoldFixer::genPOConstraints(VertexSet* ends)
{
  VertexMPVarMap* map = arrival_max_.findKey(sdc_);
  VertexNameSeq sorted_vertex = getSortedNameSeq(map);
  for (auto vertex_name : sorted_vertex) {
    Vertex* vertex = findRealVertex(vertex_name);
    if (ends->hasKey(vertex)) {
      float critical_long_arrival = getCriLongArrival(vertex);
      float min_required = sta_->vertexRequired(vertex, MinMax::min());
      // what if whole path don't satisfy hold bound
      if (fuzzyLess(critical_long_arrival, min_required)
          || complete_vio_ends_.find(vertex_name) != complete_vio_ends_.end()) {
        critical_long_arrival
            = sta_->vertexRequired(vertex, MinMax::max()) - sdc_worst_slack_;
        complete_vio_ends_.insert(vertex_name);
      }
      debugPrint(logger_,
                 utl::RSZ,
                 "LPHoldFixer",
                 3,
                 "PO {} , R = {}, r = {}",
                 vertex->name(sta_->sdcNetwork()),
                 delayAsString(critical_long_arrival, sta_, 3),
                 delayAsString(min_required, sta_, 3));
      // -A(PO) >= -R(PO)
      int index = addConstraints(-zoomIn(critical_long_arrival), LP_INFINITY);
      constraint_.findKey(index)->SetCoefficient(map->findKey(vertex_name), -1);
      // a(PO)) >= r(PO)
      index = addConstraints(zoomIn(min_required), LP_INFINITY);
      constraint_.findKey(index)->SetCoefficient(
          arrival_min_.findKey(sdc_)->findKey(vertex_name), 1);
    }
  }
}

void LPHoldFixer::genWireConstraints(WirePair wire)
{
  std::string pred = wire.first;
  std::string succ = wire.second;
  VertexMPVarMap* a_max = arrival_max_.findKey(sta_->sdc());
  VertexMPVarMap* a_min = arrival_min_.findKey(sta_->sdc());
  MPVariable* w_var = wire_.findKey(wire);
  float delay = getDelay(succ);
  debugPrint(logger_,
             utl::RSZ,
             "LPHoldFixer",
             4,
             "delay between {} -> {} : {}",
             pred.c_str(),
             succ.c_str(),
             delayAsString(delay, sta_, 3));
  // Aj >= Ai + wij + delay  -->  -Ai + Aj - wij >= delay
  int index = addConstraints(zoomIn(delay), LP_INFINITY);
  constraint_.findKey(index)->SetCoefficient(a_max->findKey(pred), -1);
  constraint_.findKey(index)->SetCoefficient(a_max->findKey(succ), 1);
  constraint_.findKey(index)->SetCoefficient(w_var, -1);
  // aj <= ai + wij + delay  -->  ai - aj + wij >= -delay
  index = addConstraints(-zoomIn(delay), LP_INFINITY);
  constraint_.findKey(index)->SetCoefficient(a_min->findKey(pred), 1);
  constraint_.findKey(index)->SetCoefficient(a_min->findKey(succ), -1);
  constraint_.findKey(index)->SetCoefficient(w_var, 1);
}

LPResult LPHoldFixer::doLP()
{
  MPObjective* const objective = solver_->MutableObjective();
  for (auto w : wire_) {
    Vertex* pred = findRealVertex(w.first.first);
    int priority = LP_PRIORITY + pred->level();
    objective->SetCoefficient(w.second, priority);
  }
  objective->SetMinimization();
  logger_->info(
      utl::RSZ, 1103, "Number of variables = {}", solver_->NumVariables());
  logger_->info(
      utl::RSZ, 1104, "Number of constraints = {}", solver_->NumConstraints());
  LPResult result = solver_->Solve();
  logger_->info(utl::RSZ,
                1105,
                "Objective value = {} (PRIORITY {})",
                objective->Value(),
                LP_PRIORITY);
  return result;
}

float LPHoldFixer::getWirePadding(WirePair wire_pair) const
{
  float padding = zoomOut(wire_.findKey(wire_pair)->solution_value());
  logger_->info(utl::RSZ,
                1106,
                "padding between {} -> {} : {}",
                wire_pair.first.c_str(),
                wire_pair.second.c_str(),
                delayAsString(padding, sta_, 3));
  return padding;
}

WirePairSet LPHoldFixer::getAllWirePairFrom(std::string vertex_name) const
{
  WirePairSet wire_pair_set;
  for (auto w : wire_) {
    WirePair wire_pair = w.first;
    if (!vertex_name.compare(wire_pair.first)) {
      wire_pair_set.insert(wire_pair);
    }
  }
  return wire_pair_set;
}

VertexNameSet LPHoldFixer::getNonZeroPaddingPred() const
{
  VertexNameSet vio_pred_set;
  for (auto w : wire_) {
    WirePair wire_pair = w.first;
    std::string pred = wire_pair.first;
    float padding = zoomOut(w.second->solution_value());
    if (fuzzyLess(LP_ROUNDING_LIMIT, padding)) {
      logger_->info(utl::RSZ,
                    1107,
                    "non zero padding between {} -> {} : {}",
                    wire_pair.first.c_str(),
                    wire_pair.second.c_str(),
                    delayAsString(padding, sta_, 3));
      if (vio_pred_set.find(pred) == vio_pred_set.end())
        vio_pred_set.insert(pred);
    }
  }
  return vio_pred_set;
}

////////////////////////////////////////////////////////
//
//  class LPResizer
//
////////////////////////////////////////////////////////

LibertyCell* LPResizer::findHoldBuffer(float min_limit)
{
  LibertyCell* max_buffer = nullptr;
  float max_delay = INF;
  for (LibertyCell* buffer : buffer_cells_) {
    float buffer_min_delay = bufferHoldDelay(buffer);
    if ((max_buffer == nullptr || buffer_min_delay < max_delay)
        && fuzzyLess(min_limit, buffer_min_delay)) {
      max_buffer = buffer;
      max_delay = buffer_min_delay;
    }
  }
  if (max_buffer)
    logger_->info(utl::RSZ,
                  1108,
                  "choose buffer {}, delay {}",
                  max_buffer->name(),
                  delayAsString(max_delay, this, 3));
  return max_buffer;
}

// function copied and updated from OpenRoad/Resizer
void LPResizer::insertHoldDelay(Vertex* drvr,
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
}

Pin* LPResizer::findFanoutPin(LPHoldFixer* fixer, WirePair wire_pair) const
{
  Vertex* pred = findRealVertex(wire_pair.first);
  Vertex* succ = findRealVertex(wire_pair.second);
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

bool LPResizer::insertBuffer(LPHoldFixer* fixer, std::string vertex_name)
{
  WirePairSet wire_pair_set = fixer->getAllWirePairFrom(vertex_name);
  PinSeq load_pins;
  Vertex* insert_from = findRealVertex(vertex_name);
  bool loads_have_out_port = false;
  float max_padding = LP_ZERO;
  float min_padding = INF;
  for (auto wire_pair : wire_pair_set) {
    float padding = fixer->getWirePadding(wire_pair);
    if (fuzzyLess(LP_ROUNDING_LIMIT, padding)) {
      Pin* fanout_pin = findFanoutPin(fixer, wire_pair);
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

  const float uncertainty = options_->uncertainty() / LP_STA_SCALING;
  if (!fuzzyLess(max_padding - min_padding, uncertainty)) {
    logger_->warn(
        utl::RSZ,
        1000,
        "padding diff in all {} fanout from {} higher then uncertainty",
        load_pins.size(),
        insert_from->name(sta_->sdcNetwork()));
    return false;
  }

  findBuffers();
  float padding = max_padding;
  int buffer_count = 1;
  LibertyCell* buffer_cell = findHoldBuffer(max_padding);
  while (!buffer_cell) {
    buffer_count *= LP_BUFFER_SPILIT;
    padding /= LP_BUFFER_SPILIT;
    buffer_cell = findHoldBuffer(padding);
  }

  if (buffer_cell != nullptr && !load_pins.empty()) {
    insertHoldDelay(
        insert_from, buffer_count, load_pins, loads_have_out_port, buffer_cell);
    logger_->info(
        utl::RSZ, 1109, "inserted {} buffer ", std::to_string(buffer_count));
  }
  return true;
}

bool LPResizer::insertAllBuffer(LPHoldFixer* fixer)
{
  bool ret = true;
  VertexNameSet vio_pred_set = fixer->getNonZeroPaddingPred();
  for (auto vertex_name : vio_pred_set) {
    if (!insertBuffer(fixer, vertex_name))
      ret = false;
  }
  logger_->info(
      utl::RSZ, 1110, "Totally inserted {} buffers", inserted_buffer_count_);
  return ret;
}

bool LPResizer::fixHoldCore()
{
  LPHoldFixer* fixer = new LPHoldFixer(openroad_, options_);
  LPResult result = fixer->doLP();
  bool ret = true;
  if (result != MPSolver::ResultStatus::OPTIMAL) {
    logger_->warn(
        utl::RSZ, 1001, "Linear Programming failed, return {}", result);
    ret = false;
  } else {
    ret = insertAllBuffer(fixer);
  }
  delete fixer;
  return ret;
}

void LPResizer::fixHold()
{
  init();
  inserted_buffer_count_ = 0;
  incrementalParasiticsBegin();
  bool ret = fixHoldCore();
  // Leave the parasitices up to date.
  updateParasitics();
  incrementalParasiticsEnd();
  if (!ret) {
    logger_->warn(utl::RSZ, 1002, "fail to fix all hold violations");
  }
}

}  // namespace rsz
