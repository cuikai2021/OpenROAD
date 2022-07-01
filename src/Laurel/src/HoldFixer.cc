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

HoldFixer::~HoldFixer()
{
  for (auto& a_max : arrival_max_)
    delete a_max.second;
  for (auto& a_min : arrival_min_)
    delete a_min.second;
}

void HoldFixer::makeVar(Name vertex_name, MMVertexMPVarMap& arrival)
{
  if (!arrival.hasKey(sdc_))
    arrival.insert(sdc_, new VertexMPVarMap());
  if (!arrival.findKey(sdc_)->hasKey(vertex_name))
    arrival.findKey(sdc_)->insert(
        vertex_name,
        solver_->MakeNumVar(
            LRL_H_ZERO, LRL_H_INFINITY, std::to_string(var_index_++)));
}

void HoldFixer::makeWire(Name pred, Name succ)
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

void HoldFixer::makeVarFanins(VertexSeq fanins)
{
  for (auto& vertex : fanins) {
    Name vertex_name = vertex->name(sta_->sdcNetwork());
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
        Name fanin_name = fanin->name(sta_->sdcNetwork());
        makeVar(fanin_name, arrival_max_);
        makeVar(fanin_name, arrival_min_);
        makeDelay(fanin);
        makeWire(fanin_name, vertex_name);
      }
    }
  }
}

void HoldFixer::findHoldVioNameMM()
{
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  int mode_cnt = 0;
  int cnt = 0;
  sta_->forEachSdc([&]() {
    mode_cnt++;
    sdc_ = sta_->sdc();
    sta_->findRequireds();
    VertexSet* ends = sta_->search()->endpoints();
    for (auto& vertex : *ends) {
      Slack slack = sta_->vertexSlack(vertex, MinMax::min());
      if (fuzzyLess(slack, slack_margin)) {
        vio_ends_name_.insert(vertex->name(sta_->sdcNetwork()));
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
  for (auto& name : vio_ends_name_)
    debugPrint(logger_, utl::LRL, "LRL", 4, "   {}", name);
}

bool HoldFixer::hasHoldViolation(Vertex* vertex) const
{
  Name name = vertex->name(sta_->sdcNetwork());
  Slack slack = sta_->vertexSlack(vertex, MinMax::min());
  if (!fuzzyInf(slack) && vio_ends_name_.find(name) != vio_ends_name_.end())
    return true;
  return false;
}

VertexSet HoldFixer::getHoldViolations()
{
  VertexSet* ends = sta_->search()->endpoints();
  VertexSet hold_failures;
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
    if (hasHoldViolation(vertex))
      hold_failures.insert(vertex);
  return hold_failures;
}

void HoldFixer::getRealVertexName()
{
  if (!sdc_name_vertex_map_.empty())
    sdc_name_vertex_map_.clear();
  VertexIterator vertex_iter(sta_->graph());
  while (vertex_iter.hasNext()) {
    Vertex* vertex = vertex_iter.next();
    Name vertex_name = vertex->name(sta_->sdcNetwork());
    sdc_name_vertex_map_.insert(vertex_name, vertex);
  }
}

Vertex* HoldFixer::findRealVertex(Name vertex_name) const
{
  return sdc_name_vertex_map_.findKey(vertex_name);
}

void HoldFixer::makeVarAllModes()
{
  findHoldVioNameMM();
  sta_->forEachSdc([&]() {
    sdc_ = sta_->sdc();
    sta_->findRequireds();
    init();
    getRealVertexName();
    VertexSet hold_failures = getHoldViolations();
    hold_failures = getAllRelatedEnds(hold_failures, options_->ignoreClk());
    if (!hold_failures.empty()) {
      VertexSet fanins = findHoldFanins(hold_failures);
      VertexSeq sorted_fanins = sortHoldFanins(fanins);
      VertexSeq sorted_hold_failures = sortHoldFanins(hold_failures);
      makeVarFanins(sorted_fanins);
      makeVarFanins(sorted_hold_failures);
      confirmAllPrimeIn();
      genPrimeOutConstraints(&hold_failures);
      genPrimeInConstraints();
    }
  });
}

void HoldFixer::confirmAllPrimeIn()
{
  VertexNameSet pi;
  VertexNameSet not_pi;
  for (auto& w : wire_) {
    const Name& pred = w.first.first;
    const Name& succ = w.first.second;
    not_pi.insert(succ);
    if (pi.hasKey(succ))
      pi.erase(succ);
    if (!not_pi.hasKey(pred))
      pi.insert(pred);
  }
  pi_name_set_ = pi;
}

bool HoldFixer::isPrimeIn(Name vertex_name) const
{
  return pi_name_set_.hasKey(vertex_name);
}

VertexNameSeq HoldFixer::getSortedNameSeq(VertexMPVarMap* mp_var_map)
{
  VertexNameSeq name_seq;
  for (auto& pair : *mp_var_map) {
    name_seq.push_back(pair.first);
  }
  std::sort(name_seq.begin(), name_seq.end());
  return name_seq;
}

// Delay HoldFixer::getArrivalMax(Vertex* vertex) const
// {
//   Delay max_arrival = LRL_H_ZERO;
//   VertexPathIterator path_iter(vertex, sta_);
//   while (path_iter.hasNext()) {
//     Path* path = path_iter.next();
//     Delay arrival = path->arrival(sta_);
//     if ((fuzzyLess(max_arrival, arrival)) && !fuzzyInf(arrival))
//       max_arrival = arrival;
//   }
//   return max_arrival;
// }

Delay HoldFixer::getCriLongArrival(Vertex* end) const
{
  Delay sum = LRL_H_ZERO;
  PathRef path = sta_->vertexWorstSlackPath(end, MinMax::max());
  PathExpanded expanded(&path, sta_);
  int start_index = expanded.startIndex();
  int path_length = expanded.size();
  for (int i = start_index; i < path_length; i++) {
    PathRef* path = expanded.path(i);
    Vertex* vertex = path->vertex(sta_);
    Name vertex_name = vertex->name(sta_->sdcNetwork());
    if (isPrimeIn(vertex_name))
      sum += getArrivalMax(vertex);
    else
      sum += getDelay(vertex->name(sta_->sdcNetwork()));
  }
  return sum;
}

void HoldFixer::makeDelay(Vertex* vertex)
{
  Name vertex_name = vertex->name(sta_->sdcNetwork());
  if (!delay_map_.hasKey(vertex_name)) {
    Delay load_delay = LRL_H_ZERO;
    PathRef path = sta_->vertexWorstSlackPath(vertex, max_);
    if (!path.vertex(sta_))
      return;
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
                   utl::LRL,
                   "LRL",
                   4,
                   "{} load_delay = {}",
                   path_vertex->name(sta_->network()),
                   delayAsString(load_delay, sta_, 3));
      }
    }
    delay_map_.insert(vertex_name, load_delay);
  }
}

Delay HoldFixer::getDelay(Name vertex_name) const
{
  if (!delay_map_.hasKey(vertex_name))
    return LRL_H_ZERO;
  return delay_map_.at(vertex_name);
}

void HoldFixer::genPrimeInConstraints()
{
  VertexMPVarMap* map = arrival_max_.findKey(sdc_);
  VertexNameSeq sorted_vertex = getSortedNameSeq(map);
  for (auto& vertex_name : sorted_vertex) {
    if (isPrimeIn(vertex_name)) {
      Vertex* vertex = findRealVertex(vertex_name);
      Delay arrival = zoomIn(getArrivalMax(vertex));
      // Delay delay = zoomIn(getDelay(vertex_name));
      debugPrint(logger_,
                 utl::LRL,
                 "LRL",
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

void HoldFixer::genPrimeOutConstraints(VertexSet* ends)
{
  VertexMPVarMap* map = arrival_max_.findKey(sdc_);
  VertexNameSeq sorted_vertex = getSortedNameSeq(map);
  for (auto& vertex_name : sorted_vertex) {
    Vertex* vertex = findRealVertex(vertex_name);
    if (ends->hasKey(vertex)) {
      Delay critical_long_arrival_value = getCriLongArrival(vertex);
      Delay max_required_value = sta_->vertexRequired(vertex, MinMax::max())
                                 - std::max(LRL_H_ZERO, sdc_worst_slack_);
      Delay critical_long_arrival
          = std::max(critical_long_arrival_value, max_required_value);
      Delay min_required = sta_->vertexRequired(vertex, MinMax::min());
      // what if whole path don't satisfy hold bound
      if (fuzzyLess(critical_long_arrival_value, min_required)
          || complete_vio_ends_.find(vertex_name) != complete_vio_ends_.end()) {
        critical_long_arrival = max_required_value;
        complete_vio_ends_.insert(vertex_name);
      }
      debugPrint(logger_,
                 utl::LRL,
                 "LRL",
                 3,
                 "PO {} , R = {}, r = {}",
                 vertex->name(sta_->sdcNetwork()),
                 delayAsString(critical_long_arrival, sta_, 3),
                 delayAsString(min_required, sta_, 3));
      // -A(PO) >= -R(PO)
      int index
          = addConstraints(-zoomIn(critical_long_arrival), LRL_H_INFINITY);
      constraint_.findKey(index)->SetCoefficient(map->findKey(vertex_name), -1);
      // a(PO)) >= r(PO)
      index = addConstraints(zoomIn(min_required), LRL_H_INFINITY);
      constraint_.findKey(index)->SetCoefficient(
          arrival_min_.findKey(sdc_)->findKey(vertex_name), 1);
    }
  }
}

void HoldFixer::genWireConstraints(WirePair wire)
{
  const Name& pred = wire.first;
  const Name& succ = wire.second;
  VertexMPVarMap* a_max = arrival_max_.findKey(sta_->sdc());
  VertexMPVarMap* a_min = arrival_min_.findKey(sta_->sdc());
  MPVariable* w_var = wire_.findKey(wire);
  Delay delay = getDelay(succ);
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             4,
             "delay between {} -> {} : {}",
             pred.c_str(),
             succ.c_str(),
             delayAsString(delay, sta_, 3));
  // Aj >= Ai + wij + delay  -->  -Ai + Aj - wij >= delay
  int index = addConstraints(zoomIn(delay), LRL_H_INFINITY);
  constraint_.findKey(index)->SetCoefficient(a_max->findKey(pred), -1);
  constraint_.findKey(index)->SetCoefficient(a_max->findKey(succ), 1);
  constraint_.findKey(index)->SetCoefficient(w_var, -1);
  // aj <= ai + wij + delay  -->  ai - aj + wij >= -delay
  index = addConstraints(-zoomIn(delay), LRL_H_INFINITY);
  constraint_.findKey(index)->SetCoefficient(a_min->findKey(pred), 1);
  constraint_.findKey(index)->SetCoefficient(a_min->findKey(succ), -1);
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

Delay HoldFixer::getWirePadding(WirePair wire_pair) const
{
  Delay padding = zoomOut(wire_.findKey(wire_pair)->solution_value());
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             4,
             "padding between {} -> {} : {}",
             wire_pair.first.c_str(),
             wire_pair.second.c_str(),
             delayAsString(padding, sta_, 3));
  return padding;
}

WirePairSet HoldFixer::getAllWirePairFrom(Name vertex_name) const
{
  WirePairSet wire_pair_set;
  for (auto& w : wire_) {
    WirePair wire_pair = w.first;
    if (!vertex_name.compare(wire_pair.first)) {
      wire_pair_set.insert(wire_pair);
    }
  }
  return wire_pair_set;
}

VertexNameSet HoldFixer::getNonZeroPaddingPred() const
{
  VertexNameSet vio_pred_set;
  for (auto& w : wire_) {
    const WirePair& wire_pair = w.first;
    const Name& pred = wire_pair.first;
    Delay padding = zoomOut(w.second->solution_value());
    if (fuzzyLess(LRL_H_ROUNDING_LIMIT, padding)) {
      debugPrint(logger_,
                 utl::LRL,
                 "LRL",
                 4,
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

}  // namespace lrl
