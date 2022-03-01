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

LPHoldFixer::LPHoldFixer(OpenRoad* openroad)
    : Resizer(), solver_(MPSolver::CreateSolver("GLOP"))
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

void LPHoldFixer::makeVar(Vertex* vertex, MMVertexMPVarMap& arrival)
{
  if (!arrival.hasKey(sdc_))
    arrival.insert(sdc_, new VertexMPVarMap());
  if (!arrival.findKey(sdc_)->hasKey(vertex)) {
    arrival.findKey(sdc_)->insert(
        vertex,
        solver_->MakeNumVar(0.0, LP_INFINITY, std::to_string(var_index_++)));
  }
}

void LPHoldFixer::makeWire(Vertex* pred, Vertex* succ)
{
  WirePair wire(pred, succ);
  if (!wire_.hasKey(wire))
    wire_.insert(
        wire,
        solver_->MakeNumVar(0.0, LP_INFINITY, std::to_string(var_index_++)));
  genWireConstraints(wire);
}

void LPHoldFixer::makeVarFanins(VertexSet fanins)
{
  for (auto vertex : fanins) {
    makeVar(vertex, arrival_max_);
    makeVar(vertex, arrival_min_);
    makeDelay(vertex);
    SearchPredNonReg2 pred(sta_);
    BfsBkwdIterator iter(BfsIndex::other, &pred, this);
    iter.enqueueAdjacentVertices(vertex);
    while (iter.hasNext()) {
      Vertex* fanin = iter.next();
      if (!fanin->isDriver(sta_->network()))
        iter.enqueueAdjacentVertices(fanin);
      else {
        makeVar(fanin, arrival_max_);
        makeVar(fanin, arrival_min_);
        makeWire(fanin, vertex);
      }
    }
  }
}

void LPHoldFixer::findHoldVioNameMM(float slack_margin)
{
  sta_->forEachSdc([&]() {
    sdc_ = sta_->sdc();
    sta_->findRequireds();
    VertexSet* ends = sta_->search()->endpoints();
    Slack worst_slack;  // unused
    VertexSet hold_failures;
    findHoldViolations(ends, 0.0f, worst_slack, hold_failures);
    for (auto vertex : hold_failures)
      vio_ends_name_.insert(vertex->name(sta_->sdcNetwork()));
  });
  for (auto name : vio_ends_name_)
    logger_->info(utl::RSZ, 1100, "vio end {}", name);
}

bool LPHoldFixer::hasHoldViolation(Vertex* vertex)
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
  for (Vertex* vertex : *ends)
    if (hasHoldViolation(vertex))
      hold_failures.insert(vertex);
  return hold_failures;
}

void LPHoldFixer::makeVarAllModes()
{
  const float slack_margin = 0.0f;
  findHoldVioNameMM(slack_margin);
  sta_->forEachSdc([&]() {
    sdc_ = sta_->sdc();
    sta_->findRequireds();
    VertexSet hold_failures = getHoldViolations();
    if (!hold_failures.empty()) {
      VertexSet fanins = findHoldFanins(hold_failures);
      makeVarFanins(fanins);
      makeVarFanins(hold_failures);
      genPOConstraints(&hold_failures);
      genPIConstraints();
    }
  });
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

bool LPHoldFixer::isPI(Vertex* vertex)
{
  for (auto w : wire_) {
    if (w.first.second == vertex)
      return false;
  }
  return true;
}

void LPHoldFixer::genPIConstraints()
{
  VertexMPVarMap* map = arrival_max_.findKey(sdc_);
  for (auto A = map->begin(); A != map->end(); ++A) {
    Vertex* vertex = A->first;
    if (isPI(vertex)) {
      float arrival = zoomIn(getArrivalMax(vertex));
      float delay = zoomIn(getDelay(vertex));
      debugPrint(logger_,
                 utl::RSZ,
                 "LPHoldFixer",
                 3,
                 "PI {} , A = {}, a = {}",
                 vertex->name(sta_->sdcNetwork()),
                 delayAsString(zoomOut(delay), sta_, 3),
                 delayAsString(zoomOut(arrival), sta_, 3));
      // A(PI) == delay_init
      int index = addConstraints(delay, delay);
      constraint_.findKey(index)->SetCoefficient(A->second, 1);
      // a(PI)) == delay_init
      index = addConstraints(arrival, arrival);
      constraint_.findKey(index)->SetCoefficient(
          arrival_min_.findKey(sdc_)->findKey(vertex), 1);
    }
  }
}

float LPHoldFixer::getRequired(Vertex* vertex, const MinMax* min_max)
{
  float extreme_required = 0.0f;
  VertexPathIterator path_iter(vertex, sta_);
  while (path_iter.hasNext()) {
    Path* path = path_iter.next();
    if (path->minMax(this) == min_max) {
      float required = path->required(sta_);
      if ((fuzzyLess(extreme_required, required)))
        extreme_required = required;
    }
  }
  return extreme_required;
}

float LPHoldFixer::getArrivalMax(Vertex* vertex)
{
  float max_arrival = 0.0f;
  VertexPathIterator path_iter(vertex, sta_);
  while (path_iter.hasNext()) {
    Path* path = path_iter.next();
    float arrival = path->arrival(sta_);
    if ((fuzzyLess(max_arrival, arrival)) && !fuzzyInf(arrival))
      max_arrival = arrival;
  }
  return max_arrival;
}

float LPHoldFixer::getCriLongArrival(Vertex* end)
{
  float sum = 0.0f;
  const Slack end_slack = sta_->vertexSlack(end, MinMax::max());
  Vertex* vertex = end;
  while (!isPI(vertex) || !vertex->isDriver(sta_->network())) {
    sum += getDelay(vertex);
    Vertex* opt_vertex = vertex;
    VertexInEdgeIterator edge_iter(opt_vertex, sta_->graph());
    while (edge_iter.hasNext()) {
      Edge* edge = edge_iter.next();
      Vertex* fanin = edge->from(sta_->graph());
      Slack slack = sta_->vertexSlack(fanin, MinMax::max());
      if (fuzzyEqual(end_slack, slack)) {
        vertex = fanin;
        break;
      }
    }
  }
  sum += getDelay(vertex);  // PI delay
  return sum;
}

void LPHoldFixer::makeDelay(Vertex* vertex)
{
  if (!delay_map_.hasKey(vertex) || isPI(vertex)) {
    Delay load_delay = 0.0f;
    PathRef path = sta_->vertexWorstSlackPath(vertex, max_);
    PathExpanded expanded(&path, sta_);
    const DcalcAnalysisPt* dcalc_ap = path.dcalcAnalysisPt(sta_);
    int lib_ap = dcalc_ap->libertyIndex();
    int start_index = expanded.startIndex();
    int path_length = expanded.size();
    for (int i = start_index; i < path_length; i++) {
      PathRef* path = expanded.path(i);
      Vertex* path_vertex = path->vertex(sta_);
      if (vertex == path_vertex) {
        TimingArc* prev_arc = expanded.prevArc(i);
        // TimingArc* corner_arc = prev_arc->cornerArc(lib_ap);
        Edge* prev_edge = path->prevEdge(prev_arc, sta_);
        load_delay = sta_->graph()->arcDelay(prev_edge, prev_arc, dcalc_ap->index());
        // Remove intrinsic delay to find load dependent delay.
        //  - corner_arc->intrinsicDelay();
        debugPrint(logger_,
                   utl::RSZ,
                   "LPHoldFixer",
                   4,
                   "{} load_delay = {}",
                   path_vertex->name(sta_->network()),
                   delayAsString(load_delay, sta_, 3));
      }
    }
    std::pair<std::string, float> delay_pair
        = {vertex->name(sta_->sdcNetwork()), load_delay};
    delay_map_.insert(vertex, delay_pair);
  }
}

float LPHoldFixer::getDelay(Vertex* vertex)
{
  if (!delay_map_.hasKey(vertex))
    return 0.0f;
  return delay_map_[vertex].second;
}

std::string LPHoldFixer::getVertexName(Vertex* vertex)
{
  if (!delay_map_.hasKey(vertex))
    return "unknown";
  return delay_map_[vertex].first;
}

void LPHoldFixer::genPOConstraints(VertexSet* ends)
{
  VertexMPVarMap* map = arrival_max_.findKey(sdc_);
  for (auto A = map->begin(); A != map->end(); ++A) {
    Vertex* vertex = A->first;
    if (ends->hasKey(vertex)) {
      // -A(PO) >= -R(PO)
      float critical_long_arrival = getCriLongArrival(vertex);
      float min_required = getRequired(vertex, MinMax::min());
      debugPrint(logger_,
                 utl::RSZ,
                 "LPHoldFixer",
                 3,
                 "PO {} , R = {}, r = {}",
                 vertex->name(sta_->sdcNetwork()),
                 delayAsString(critical_long_arrival, sta_, 3),
                 delayAsString(min_required, sta_, 3));
      int index = addConstraints(-zoomIn(critical_long_arrival), LP_INFINITY);
      constraint_.findKey(index)->SetCoefficient(A->second, -1);
      // a(PO)) >= r(PO)
      index = addConstraints(zoomIn(getRequired(vertex, MinMax::min())),
                             LP_INFINITY);
      constraint_.findKey(index)->SetCoefficient(
          arrival_min_.findKey(sdc_)->findKey(vertex), 1);
    }
  }
}

void LPHoldFixer::genWireConstraints(WirePair wire)
{
  Vertex* pred = wire.first;
  Vertex* succ = wire.second;
  VertexMPVarMap* a_max = arrival_max_.findKey(sta_->sdc());
  VertexMPVarMap* a_min = arrival_min_.findKey(sta_->sdc());
  MPVariable* w_var = wire_.findKey(wire);
  float delay = getDelay(succ);
  debugPrint(logger_,
             utl::RSZ,
             "LPHoldFixer",
             4,
             "delay between {} -> {} : {}",
             pred->name(sta_->sdcNetwork()),
             succ->name(sta_->sdcNetwork()),
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
    Vertex* pred = w.first.first;
    double priority = 1.0 + pred->level() * LP_PRIORITY;
    objective->SetCoefficient(w.second, priority);
  }
  objective->SetMinimization();
  logger_->info(
      utl::RSZ, 1102, "Number of variables = {}", solver_->NumVariables());
  logger_->info(
      utl::RSZ, 1103, "Number of constraints = {}", solver_->NumConstraints());
  LPResult result = solver_->Solve();
  logger_->info(utl::RSZ, 1104, "Objective value = {}", objective->Value());
  return result;
}

float LPHoldFixer::getWirePadding(WirePair wire_pair)
{
  float padding = zoomOut(wire_.findKey(wire_pair)->solution_value());
  debugPrint(logger_,
             utl::RSZ,
             "LPHoldFixer",
             3,
             "padding between {} -> {} : {}",
             getVertexName(wire_pair.first),
             getVertexName(wire_pair.second),
             delayAsString(padding, sta_, 3));
  return padding;
}

WirePairSet LPHoldFixer::getAllWirePairFrom(Vertex* vertex)
{
  WirePairSet wire_pair_set;
  for (auto w : wire_) {
    WirePair wire_pair = w.first;
    if (wire_pair.first == vertex) {
      wire_pair_set.insert(wire_pair);
    }
  }
  return wire_pair_set;
}

VertexSet LPHoldFixer::getNonZeroPaddingPred()
{
  VertexSet vio_pred_set;
  for (auto w : wire_) {
    WirePair wire_pair = w.first;
    Vertex* pred = wire_pair.first;
    float padding = zoomOut(w.second->solution_value());
    if (fuzzyLess(LP_ROUNDING_LIMIT, padding)) {
      logger_->info(utl::RSZ,
                    1105,
                    "non zero padding between {} -> {} : {}",
                    getVertexName(wire_pair.first),
                    getVertexName(wire_pair.second),
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
  logger_->info(utl::RSZ,
                1106,
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
  Net* drvr_net = sta_->network()->isTopLevelPort(drvr_pin)
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
    buffer
        = sta_->getDbNetwork()->makeInstance(buffer_cell, buffer_name.c_str(), parent);
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
  Pin* buffer_out_pin = sta_->network()->findPin(buffer, output);
  // resizeToTargetSlew(buffer_out_pin, false);
}

Vertex* LPResizer::findRealVertex(LPHoldFixer* fixer, Vertex* vertex)
{
  std::string vertex_name = fixer->getVertexName(vertex);
  VertexIterator vertex_iter(sta_->graph());
  while (vertex_iter.hasNext()) {
    Vertex* test_vertex = vertex_iter.next();
    if (!vertex_name.compare(test_vertex->name(sta_->sdcNetwork())))
      return test_vertex;
  }
  return nullptr;
}

Pin* LPResizer::findFanoutPin(LPHoldFixer* fixer, WirePair wire_pair)
{
  Vertex* pred = findRealVertex(fixer, wire_pair.first);
  Vertex* succ = findRealVertex(fixer, wire_pair.second);
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

bool LPResizer::insertBuffer(LPHoldFixer* fixer, Vertex* vertex)
{
  WirePairSet wire_pair_set = fixer->getAllWirePairFrom(vertex);
  PinSeq load_pins;
  bool loads_have_out_port = false;
  bool ret = false;
  float max_padding = -0.0f;
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
  findBuffers();
  LibertyCell* buffer_cell = findHoldBuffer(max_padding);
  Vertex* insert_from = findRealVertex(fixer, vertex);
  if (buffer_cell != nullptr && !load_pins.empty()) {
    const DcalcAnalysisPt* dcalc_ap
        = sta_->cmdCorner()->findDcalcAnalysisPt(max_);
    insertHoldDelay(
        insert_from, 1, load_pins, loads_have_out_port, buffer_cell);
    if (!fuzzyLess(max_padding - min_padding, uncertainty_ / LP_STA_SCALING)) {
      logger_->warn(
          utl::RSZ,
          1000,
          "can not insert one buffer to satisfy all {} fanout from {}",
          load_pins.size(),
          insert_from->name(sta_->sdcNetwork()));
    } else
      ret = true;
  } else {
    logger_->warn(utl::RSZ,
                  1001,
                  "No matched buffer, min padding is {}",
                  delayAsString(min_padding, this, 3));
  }
  return ret;
}

bool LPResizer::insertAllBuffer(LPHoldFixer* fixer)
{
  bool ret = true;
  VertexSet vio_pred_set = fixer->getNonZeroPaddingPred();
  for (auto vertex : vio_pred_set) {
    if (!insertBuffer(fixer, vertex))
      ret = false;
  }
  logger_->info(utl::RSZ, 1101, "inserted {} buffers", inserted_buffer_count_);
  return ret;
}

bool LPResizer::fixHoldCore()
{
  LPHoldFixer* fixer = new LPHoldFixer(openroad_);
  LPResult result = fixer->doLP();
  bool ret = true;
  if (result != MPSolver::ResultStatus::OPTIMAL) {
    logger_->warn(
        utl::RSZ, 1002, "Linear Programming failed, return {}", result);
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
    logger_->warn(utl::RSZ, 1003, "fail to fix all hold violations");
  }
}

}  // namespace rsz
