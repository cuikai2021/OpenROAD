#include "rsz/LPResizer.hh"

namespace rsz {

using namespace operations_research;
using namespace sta;

////////////////////////////////////////////////////////
//
//  class LPHoldFixer
//
////////////////////////////////////////////////////////

LPHoldFixer::LPHoldFixer()
    : Resizer(), solver_(MPSolver::CreateSolver("GLHoldFixer"))
{
  makeVarAllModes();
  genPIConstraints();
  genPOConstraints();
  genWireConstraints();
}

const char* LPHoldFixer::getVarName(Vertex* vertex, const char* fmt)
{
  char* buf;
  sprintf(buf, fmt, vertex->name(sdc_network_));
  return buf;
}

const char* LPHoldFixer::getWireName(Vertex* pred, Vertex* succ)
{
  char* buf;
  sprintf(
      buf, ARR_WIRE_NAME, pred->name(sdc_network_), succ->name(sdc_network_));
  return buf;
}

void LPHoldFixer::makeVar(Vertex* vertex)
{
  if (!arrival_max_.hasKey(sdc_))
    arrival_max_.insert(sdc_, VertexMPVarMap());
  if (!arrival_max_[sdc_].hasKey(vertex)) {
    const char* var_name = getVarName(vertex, ARR_MAX_NAME);
    arrival_max_[sdc_].insert(vertex,
                              solver_->MakeNumVar(0.0, LP_INFINITY, var_name));
  }

  if (!arrival_min_.hasKey(sdc_))
    arrival_min_.insert(sdc_, VertexMPVarMap());
  if (!arrival_min_[sdc_].hasKey(vertex)) {
    const char* var_name = getVarName(vertex, ARR_MIN_NAME);
    arrival_min_[sdc_].insert(vertex,
                              solver_->MakeNumVar(0.0, LP_INFINITY, var_name));
  }
}

void LPHoldFixer::makeWire(Vertex* pred, Vertex* succ)
{
  const char* wire_name = getWireName(pred, succ);
  WirePair wire(pred, succ);

  if (!wire_list_.hasKey(sdc_))
    wire_list_.insert(sdc_, WirePairSet());

  if (!wire_.hasKey(wire))
    wire_.insert(wire, solver_->MakeNumVar(0.0, LP_INFINITY, wire_name));
  wire_list_[sdc_].insert(wire);
}

void LPHoldFixer::makeVarFanins(VertexSet fanins)
{
  for (auto vertex : fanins) {
    makeVar(vertex);
    SearchPredNonReg2 pred(sta_);
    BfsBkwdIterator iter(BfsIndex::other, &pred, this);
    iter.enqueueAdjacentVertices(vertex);
    while (iter.hasNext()) {
      Vertex* pred = iter.next();
      makeWire(pred, vertex);
    }
  }
}

void LPHoldFixer::makeVarAllModes()
{
  sta_->forEachSdc([&]() {
    LibertyCell* buffer_cell = findHoldBuffer();
    sta_->findRequireds();
    VertexSet* ends = sta_->search()->endpoints();
    Slack worst_slack;  // unused
    VertexSet hold_failures;
    findHoldViolations(ends, 0.0f, worst_slack, hold_failures);
    debugPrint(
        logger_, utl::RSZ, "LPHoldFixer", 3, "worst slack: {}", worst_slack);
    VertexSet fanins = findHoldFanins(hold_failures);
    makeVarFanins(fanins);
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
  SearchPredNonReg2 search_pred(sta_);
  VertexInEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge* edge = edge_iter.next();
    Vertex* from_vertex = edge->from(graph_);
    if (search_pred.searchFrom(from_vertex) && search_pred.searchThru(edge))
      return false;
  }
  return true;
}

void LPHoldFixer::genPIConstraints()
{
  sta_->forEachSdc([&](Sdc* sdc) {
    for (auto A : arrival_max_[sdc]) {
      Vertex* vertex = A.first;
      if (isPI(vertex)) {
        // A(PI) == 0.0
        int index = addConstraints(0.0f, 0.0f);
        constraint_.findKey(index)->SetCoefficient(A.second, 1);
        // a(PI)) == 0.0
        index = addConstraints(0.0f, 0.0f);
        constraint_.findKey(index)->SetCoefficient(
            arrival_min_[sdc].findKey(vertex), 1);
      }
    }
  });
}

float LPHoldFixer::getRequired(Vertex* vertex, const MinMax* min_max)
{
  float extreme_required = INF;
  sta_->findRequireds();
  VertexPathIterator path_iter(vertex, sta_);
  while (path_iter.hasNext()) {
    Path* path = path_iter.next();
    if (path->minMax(this) == min_max) {
      float required = path->required(sta_);
      if ((min_max == MinMax::max() ^ fuzzyLess(required, extreme_required))
          || fuzzyInf(extreme_required))
        extreme_required = required;
    }
  }
  return extreme_required;
}

//
//  -----> I [cell] Q ------> I [cell] Q ---->
//                  |         |        |
//                pred      fanin     succ
//

float LPHoldFixer::getDelay(WirePair wire)
{
  sta_->findRequireds();
  Vertex* pred = wire.first;
  Vertex* succ = wire.second;
  VertexPathIterator path_iter(succ, sta_);
  while (path_iter.hasNext()) {
    Path* path = path_iter.next();
    PathVertex succ_path(succ_path);
    PathVertex fanin_path;
    PathVertex pred_path;
    TimingArc* prev_arc;
    succ_path.prevPath(sta_, fanin_path, prev_arc);
    fanin_path.prevPath(sta_, pred_path, prev_arc);
    if (pred_path.vertex(graph_) == pred) {
      float arrival_succ = succ_path.arrival(sta_);
      float arrival_pred = pred_path.arrival(sta_);
      return arrival_succ - arrival_pred;
    }
  }
  debugPrint(logger_,
             utl::RSZ,
             "LPHoldFixer",
             1,
             "can not get delay between {} -> {}",
             pred->name(sdc_network_),
             succ->name(sdc_network_));
  return INF;
}

void LPHoldFixer::genPOConstraints()
{
  sta_->forEachSdc([&]() {
    for (auto A : arrival_max_[sdc_]) {
      Vertex* vertex = A.first;
      if (sta_->search()->isEndpoint(vertex)) {
        // -A(PO) >= -R(PO)
        int index = addConstraints(0.0f - getRequired(vertex, MinMax::max()),
                                   LP_INFINITY);
        constraint_.findKey(index)->SetCoefficient(A.second, -1);
        // a(PO)) >= r(PO)
        index = addConstraints(getRequired(vertex, MinMax::min()), LP_INFINITY);
        constraint_.findKey(index)->SetCoefficient(
            arrival_min_[sdc_].findKey(vertex), 1);
      }
    }
  });
}

void LPHoldFixer::genWireConstraints()
{
  sta_->forEachSdc([&]() {
    for (auto wire : wire_list_[sdc_]) {
      Vertex* pred = wire.first;
      Vertex* succ = wire.second;
      VertexMPVarMap A = arrival_max_[sdc_];
      VertexMPVarMap a = arrival_max_[sdc_];
      MPVariable* w_var = wire_.findKey(wire);
      float delay = getDelay(wire);
      // Aj >= Ai + wij + delay  -->  -Ai + Aj - wij >= delay
      int index = addConstraints(delay, LP_INFINITY);
      constraint_.findKey(index)->SetCoefficient(A[pred], -1);
      constraint_.findKey(index)->SetCoefficient(A[succ], 1);
      constraint_.findKey(index)->SetCoefficient(w_var, -1);
      // aj <= ai + wij + delay  -->  ai - aj + wij >= -delay
      index = addConstraints(0.0f - delay, LP_INFINITY);
      constraint_.findKey(index)->SetCoefficient(a[pred], 1);
      constraint_.findKey(index)->SetCoefficient(a[succ], -1);
      constraint_.findKey(index)->SetCoefficient(w_var, 1);
    }
  });
}

LPResult LPHoldFixer::doLP()
{
  MPObjective* const objective = solver_->MutableObjective();
  for (auto w : wire_) {
    objective->SetCoefficient(w.second, 1);
  }
  objective->SetMinimization();
  return solver_->Solve();
}

float LPHoldFixer::getWirePadding(WirePair wire_pair)
{
  return wire_.findKey(wire_pair)->solution_value();
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
    float padding = w.second->solution_value();
    if (vio_pred_set.find(pred) != vio_pred_set.end()
        && fuzzyLess(0.0f, padding))
      vio_pred_set.insert(pred);
  }
  return vio_pred_set;
}

////////////////////////////////////////////////////////
//
//  class LPResizer
//
////////////////////////////////////////////////////////

LibertyCell* LPResizer::findHoldBuffer(float min_limit, float max_limit)
{
  LibertyCell* max_buffer = nullptr;
  float max_delay = min_limit;
  for (LibertyCell* buffer : buffer_cells_) {
    float buffer_min_delay = bufferHoldDelay(buffer);
    if (max_buffer == nullptr
        || (buffer_min_delay > max_delay && buffer_min_delay <= max_limit)) {
      max_buffer = buffer;
      max_delay = buffer_min_delay;
    }
  }
  return max_buffer;
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
    if (fuzzyLess(0.0f, padding)) {
      Pin* fanout_pin = wire_pair.second->pin();
      load_pins.push_back(fanout_pin);
      if (network_->direction(fanout_pin)->isAnyOutput()
          && network_->isTopLevelPort(fanout_pin))
        loads_have_out_port = true;

      if (fuzzyLess(max_padding, padding))
        max_padding = padding;
      if (fuzzyLess(padding, min_padding) || fuzzyInf(min_padding))
        min_padding = padding;
    }
  }
  LibertyCell* buffer_cell
      = findHoldBuffer(min_padding - uncertainty_, min_padding + uncertainty_);
  if (buffer_cell != nullptr) {
    makeHoldDelay(vertex, 1, load_pins, loads_have_out_port, buffer_cell);
    inserted_buffer_count_++;
    if (!fuzzyLess(max_padding - min_padding, uncertainty_)) {
      logger_->warn(
          utl::RSZ,
          1001,
          "can not insert one buffer to satisfy all {} fanout from {}, "
          "uncertainty is {}",
          load_pins.size(),
          vertex->name(sdc_network_),
          delayAsString(uncertainty_, this, 3));
    }
    ret = true;
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
  LPHoldFixer* fixer = new LPHoldFixer();
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
  findBuffers();
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
