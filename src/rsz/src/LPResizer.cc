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
  makeVarAllModes();
  genPIConstraints();
  genPOConstraints();
  genWireConstraints();
}

LPHoldFixer::~LPHoldFixer()
{
  for (auto w : wire_list_)
    delete w.second;
  for (auto A : arrival_max_)
    delete A.second;
  for (auto a : arrival_min_)
    delete a.second;
}

void LPHoldFixer::makeVar(Vertex* vertex)
{
  if (!arrival_max_.hasKey(sdc_))
    arrival_max_.insert(sdc_, new VertexMPVarMap());
  if (!arrival_max_.findKey(sdc_)->hasKey(vertex)) {
    arrival_max_.findKey(sdc_)->insert(
        vertex,
        solver_->MakeNumVar(0.0, LP_INFINITY, std::to_string(var_index_++)));
  }

  if (!arrival_min_.hasKey(sdc_))
    arrival_min_.insert(sdc_, new VertexMPVarMap());
  if (!arrival_min_.findKey(sdc_)->hasKey(vertex)) {
    arrival_min_.findKey(sdc_)->insert(
        vertex,
        solver_->MakeNumVar(0.0, LP_INFINITY, std::to_string(var_index_++)));
  }
}

void LPHoldFixer::makeWire(Vertex* pred, Vertex* succ)
{
  WirePair wire(pred, succ);

  if (!wire_list_.hasKey(sdc_))
    wire_list_.insert(sdc_, new WirePairSet());

  if (!wire_.hasKey(wire))
    wire_.insert(
        wire,
        solver_->MakeNumVar(0.0, LP_INFINITY, std::to_string(wire_index_++)));
  wire_list_.findKey(sdc_)->insert(wire);
  printf(
      "pair: %s -> %s\n", pred->name(sdc_network_), succ->name(sdc_network_));
}

void LPHoldFixer::makeVarFanins(VertexSet fanins)
{
  for (auto vertex : fanins) {
    makeVar(vertex);
    SearchPredNonReg2 pred(sta_);
    BfsBkwdIterator iter(BfsIndex::other, &pred, this);
    iter.enqueueAdjacentVertices(vertex);
    while (iter.hasNext()) {
      Vertex* fanin = iter.next();
      if (!fanin->isDriver(network_))
        iter.enqueueAdjacentVertices(fanin);
      else
        makeWire(fanin, vertex);
    }
  }
}

void LPHoldFixer::makeVarAllModes()
{
  sta_->forEachSdc([&]() {
    sdc_ = sta_->sdc();
    sta_->findRequireds();
    VertexSet* ends = sta_->search()->endpoints();
    Slack worst_slack;  // unused
    VertexSet hold_failures;
    findHoldViolations(ends, 0.0f, worst_slack, hold_failures);
    if (!hold_failures.empty()) {
      debugPrint(logger_,
                 utl::RSZ,
                 "LPHoldFixer",
                 3,
                 "worst slack: {}",
                 delayAsString(worst_slack, this, 3));
      VertexSet fanins = findHoldFanins(hold_failures);
      makeVarFanins(fanins);
      makeVarFanins(hold_failures);
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
  SearchPredNonReg2 search_pred(sta_);
  VertexInEdgeIterator edge_iter(vertex, sta_->graph());
  while (edge_iter.hasNext()) {
    Edge* edge = edge_iter.next();
    Vertex* from_vertex = edge->from(sta_->graph());
    if (search_pred.searchFrom(from_vertex) && search_pred.searchThru(edge))
      return false;
  }
  return true;
}

void LPHoldFixer::genPIConstraints()
{
  sta_->forEachSdc([&](Sdc* sdc) {
    sdc_ = sta_->sdc();
    VertexMPVarMap* map = arrival_max_.findKey(sdc_);
    for (auto A = map->begin(); A != map->end(); ++A) {
      Vertex* vertex = A->first;
      if (isPI(vertex)) {
        // A(PI) == 0.0
        int index = addConstraints(0.0f, 0.0f);
        constraint_.findKey(index)->SetCoefficient(A->second, 1);
        // a(PI)) == 0.0
        index = addConstraints(0.0f, 0.0f);
        constraint_.findKey(index)->SetCoefficient(
            arrival_min_.findKey(sdc_)->findKey(vertex), 1);
      }
    }
  });
}

float LPHoldFixer::getRequired(Vertex* vertex, const MinMax* min_max)
{
  float extreme_required = INF;
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

float LPHoldFixer::getDelay(WirePair wire)
{
  Vertex* succ = wire.second;
  Pin* pin = succ->pin();
  LibertyPort* drvr_port = sta_->network()->libertyPort(pin);
  return gateDelay(drvr_port, 0.0f, tgt_slew_dcalc_ap_);
}

void LPHoldFixer::genPOConstraints()
{
  sta_->forEachSdc([&]() {
    init();
    sdc_ = sta_->sdc();
    sta_->findRequireds();
    VertexSet* ends = sta_->search()->endpoints();
    VertexMPVarMap* map = arrival_max_.findKey(sdc_);
    for (auto A = map->begin(); A != map->end(); ++A) {
      Vertex* vertex = A->first;
      if (ends->hasKey(vertex)) {
        // -A(PO) >= -R(PO)
        int index = addConstraints(0.0f - getRequired(vertex, MinMax::max()),
                                   LP_INFINITY);
        constraint_.findKey(index)->SetCoefficient(A->second, -1);
        // a(PO)) >= r(PO)
        index = addConstraints(getRequired(vertex, MinMax::min()), LP_INFINITY);
        constraint_.findKey(index)->SetCoefficient(
            arrival_min_.findKey(sdc_)->findKey(vertex), 1);
      }
    }
  });
}

void LPHoldFixer::genWireConstraints()
{
  sta_->forEachSdc([&]() {
    init();
    sdc_ = sta_->sdc();
    sta_->findRequireds();
    WirePairSet* map = wire_list_.findKey(sdc_);
    VertexSet* ends = sta_->search()->endpoints();
    for (auto iter = map->begin(); iter != map->end(); ++iter) {
      WirePair wire = *iter;
      Vertex* pred = wire.first;
      Vertex* succ = wire.second;
      VertexMPVarMap* A = arrival_max_.findKey(sdc_);
      VertexMPVarMap* a = arrival_max_.findKey(sdc_);
      MPVariable* w_var = wire_.findKey(wire);
      float delay = 0.0f;
      if (!ends->hasKey(succ))
        float delay = getDelay(wire);
      // Aj >= Ai + wij + delay  -->  -Ai + Aj - wij >= delay
      int index = addConstraints(delay, LP_INFINITY);
      constraint_.findKey(index)->SetCoefficient(A->findKey(pred), -1);
      constraint_.findKey(index)->SetCoefficient(A->findKey(succ), 1);
      constraint_.findKey(index)->SetCoefficient(w_var, -1);
      // aj <= ai + wij + delay  -->  ai - aj + wij >= -delay
      index = addConstraints(0.0f - delay, LP_INFINITY);
      constraint_.findKey(index)->SetCoefficient(a->findKey(pred), 1);
      constraint_.findKey(index)->SetCoefficient(a->findKey(succ), -1);
      constraint_.findKey(index)->SetCoefficient(w_var, 1);
    }
  });
}

LPResult LPHoldFixer::doLP()
{
  MPObjective* const objective = solver_->MutableObjective();
  for (auto w : wire_) {
    printf("pair: %s -> %s\n",
           w.first.first->name(sdc_network_),
           w.first.second->name(sdc_network_));
    objective->SetCoefficient(w.second, 1);
  }
  objective->SetMinimization();
  return solver_->Solve();
}

float LPHoldFixer::getWirePadding(WirePair wire_pair)
{
  float padding = wire_.findKey(wire_pair)->solution_value();
  debugPrint(logger_,
             utl::RSZ,
             "LPHoldFixer",
             3,
             "padding between {} -> {} : {}",
             wire_pair.first->name(sta_->sdcNetwork()),
             wire_pair.second->name(sta_->sdcNetwork()),
             padding);
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
    float padding = w.second->solution_value();
    debugPrint(logger_,
               utl::RSZ,
               "LPHoldFixer",
               3,
               "padding between {} -> {} : {}",
               pred->name(sta_->sdcNetwork()),
               wire_pair.second->name(sta_->sdcNetwork()),
               padding);
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
          vertex->name(sta_->sdcNetwork()),
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
