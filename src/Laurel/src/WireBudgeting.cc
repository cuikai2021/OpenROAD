#include "lrl/Laurel.hh"
#include "lrl/SpefReader.hh"
#include "sta/ParasiticsClass.hh"

namespace lrl {

using namespace sta;

////////////////////////////////////////////////////////
//
//  class WireBudgeting
//
////////////////////////////////////////////////////////

void WireBudgeting::initAlgorithm()
{
  if (makeDelaySpef() && makeDelaySpefFct()) {
    VertexSet* ends = sta_->search()->endpoints();
    VertexSet failures;
    int cnt = 0;
    for (Vertex* vertex : *ends) {
      Slack slack = sta_->vertexSlack(vertex, MinMax::max());
      if (!fuzzyInf(slack)) {
        failures.insert(vertex);
        cnt++;
      }
    }
    logger_->info(utl::LRL, 1402, "detected {} Budgeting Ends", cnt);
    buildIndexMap(failures);
  }
}

void WireBudgeting::buildAllFaninEdge(const VertexSet& fanins)
{
  for (auto& vertex : fanins) {
    bool is_pi = true;
    bellman_max_iter_num_
        = std::max(vertex->level() / 20, bellman_max_iter_num_);
    buildIndex(vertex);
    Name vertex_name = vertex->name(sta_->sdcNetwork());
    name_vertex_map_.insert(vertex_name, vertex);
    pin_vertex_map_.insert(vertex->pin(), vertex);
    SearchPredNonReg2 s_pred(sta_);
    BfsBkwdIterator iter(BfsIndex::other, &s_pred, this);
    iter.enqueueAdjacentVertices(vertex);
    while (iter.hasNext()) {
      Vertex* fanin = iter.next();
      Name fanin_name = fanin->name(sta_->sdcNetwork());
      name_vertex_map_.insert(fanin_name, fanin);
      pin_vertex_map_.insert(fanin->pin(), fanin);
      if (!fanin->isDriver(sta_->network())) {
        BfsBkwdIterator fanin_iter(BfsIndex::other, &s_pred, this);
        fanin_iter.enqueueAdjacentVertices(fanin);
        while (fanin_iter.hasNext()) {
          Vertex* pred = fanin_iter.next();
          if (pred->isDriver(sta_->network())) {
            buildIndex(pred);
            is_pi = false;
            Name pred_name = pred->name(sta_->sdcNetwork());
            name_vertex_map_.insert(pred_name, pred);
            pin_vertex_map_.insert(pred->pin(), pred);
            WirePair wire(pred_name, fanin_name);
            WirePair cal_wire(pred_name, vertex_name);
            wire_fanout_map_.insert(wire, vertex);
            distance_map_.insert(cal_wire, getTreeDist(pred, fanin));
            addConstraintEdge(pred, vertex);
            opt_wires_.insert(wire);
          }
        }
      } else {
        buildIndex(fanin);
        is_pi = false;
        WirePair wire(fanin_name, vertex_name);
        wire_fanout_map_.insert(wire, vertex);
        addConstraintEdge(fanin, vertex);
        opt_wires_.insert(wire);
      }
    }
    if (is_pi)
      addConstraintPrimeIn(vertex);
  }
}

WirePair WireBudgeting::getRealWire(const WirePair& wire)
{
  if (!wire_fanout_map_.hasKey(wire))
    return wire;
  Vertex* fanout = wire_fanout_map_.findKey(wire);
  Name fanout_name = fanout->name(sta_->sdcNetwork());
  return WirePair(wire.first, fanout_name);
}

Vertex* WireBudgeting::getRealFanoutVertex(const WirePair& wire)
{
  if (!wire_fanout_map_.hasKey(wire))
    return getVertex(wire.second);
  return wire_fanout_map_.findKey(wire);
}

Delay WireBudgeting::costSlope(Vertex* pred, Vertex* succ)
{
  uint64 wire_length = getTreeDist(pred, succ);
  Name succ_name = succ->name(sdc_network_);
  Delay min_delay = LRL_H_ZERO;
  Delay max_delay = LRL_H_ZERO;

  if (spef_delay_.hasKey(succ_name))
    min_delay = spef_delay_.at(succ_name);
  if (spef_delay_factor_.hasKey(succ_name))
    max_delay = spef_delay_factor_.at(succ_name);

  debugPrint(logger_,
             utl::LRL,
             "LRL",
             3,
             "{}  len: {}, max delay: {} , min delay: {}",
             succ_name.c_str(),
             wire_length,
             delayAsString(max_delay, sta_, 3),
             delayAsString(min_delay, sta_, 3));
  if (fuzzyZero(max_delay) || fuzzyLessEqual(max_delay, min_delay))
    return LRL_H_INFINITY;
  return wire_length / (max_delay - min_delay) / LRL_H_STA_SCALING
         / LRL_H_SCALING / LRL_H_SCALING / LRL_H_SCALING;
}

Delay WireBudgeting::getLowBound(Vertex* pred, Vertex* succ)
{
  Delay delay = LRL_H_ZERO;
  Name succ_name = succ->name(sta_->sdcNetwork());
  if (spef_delay_.hasKey(succ_name))
    delay = spef_delay_.at(succ_name);
  return delay;
}

Delay WireBudgeting::getHighBound(Vertex* pred, Vertex* succ)
{
  Delay delay = LRL_H_ZERO;
  Name succ_name = succ->name(sta_->sdcNetwork());
  if (spef_delay_.hasKey(succ_name))
    delay = spef_delay_factor_.at(succ_name);
  return delay;
}

void WireBudgeting::addConstraintEdge(Vertex* pred, Vertex* succ)
{
  Index pred_index = getIndex(pred);
  Index succ_index = getIndex(succ);
  Cost capacity = delay2Cost(costSlope(pred, succ));
  Delay low_bound = getLowBound(pred, succ);
  Delay high_bound = getHighBound(pred, succ);
  Delay& delay = high_bound;

  addConstraintBase(pred_index, succ_index, capacity, -delay2Cost(delay));
  addConstraintBase(
      pred_index, succ_index, LRL_S_INT_INF, -delay2Cost(low_bound));
  // addConstraintBase(
  //     succ_index, pred_index, LRL_S_INT_INF, delay2Cost(high_bound));
}

SteinerTree* WireBudgeting::getTree(Vertex* drvr)
{
  if (drvr_tree_map_.hasKey(drvr))
    return drvr_tree_map_.findKey(drvr);

  SteinerTree* tree = makeSteinerTree(drvr->pin(),
                                      true,
                                      max_steiner_pin_count_,
                                      stt_builder_,
                                      sta_->getDbNetwork(),
                                      logger_);
  drvr_tree_map_.insert(drvr, tree);
  return tree;
}

uint64 WireBudgeting::getTreeDist(Vertex* drvr, Vertex* dst)
{
  Name drvr_name = drvr->name(sdc_network_);
  Name dst_name = dst->name(sdc_network_);
  WirePair wire(drvr_name, dst_name);
  if (!distance_map_.hasKey(wire)) {
    uint64 wire_length = LRL_S_ZERO;
    SteinerTree* tree = getTree(drvr);
    if (tree) {
      SteinerPt cur = tree->drvrPt(sta_->network());
      SteinerPt last = SteinerTree::null_pt;
      SteinerPt next = findNextPt(tree, cur, last);
      Point dst_pt = sta_->getDbNetwork()->location(dst->pin());
      while (dst_pt != tree->location(cur)) {
        if (next == SteinerTree::null_pt)
          break;
        Point cur_pt = tree->location(cur);
        Point next_pt = tree->location(next);
        wire_length += Point::manhattanDistance(cur_pt, next_pt);
        last = cur;
        cur = next;
        next = findNextPt(tree, cur, last);
      }
    }
    if (wire_length == LRL_S_ZERO) {
      // if pin count < 2, it would not create a steiner tree
      // use manhattan distance directly
      Point drvr_pt = sta_->getDbNetwork()->location(drvr->pin());
      Point dst_pt = sta_->getDbNetwork()->location(dst->pin());
      wire_length = Point::manhattanDistance(drvr_pt, dst_pt);
    }
    distance_map_.insert(wire, wire_length);
  }
  return distance_map_.at(wire);
}

SteinerPt WireBudgeting::findNextPt(SteinerTree* tree,
                                    const SteinerPt& drvr_pt,
                                    const SteinerPt& last)
{
  SteinerPt left = tree->left(drvr_pt);
  if (left == last)
    return tree->right(drvr_pt);
  return left;
}

void WireBudgeting::buildSpefDelayMap(sta::Map<Name, Delay>& spef_delay_map)
{
  sta_->findRequireds();
  VertexIterator vertex_iter(sta_->graph());
  while (vertex_iter.hasNext()) {
    Vertex* vertex = vertex_iter.next();
    if (!vertex->isRoot() && vertex->isDriver(sta_->network())) {
      Delay delay = getCellDelay(vertex);
      Name vertex_name = vertex->name(sta_->sdcNetwork());
      spef_delay_map.insert(vertex_name, delay);
    }
  }
}

bool WireBudgeting::makeDelaySpefCore(sta::Map<Name, Delay>& map, double factor)
{
  bool ret = sta::readSpefFct(options_->spefFileName(),
                              sta_->getDbNetwork()->topInstance(),
                              sta_->cmdCorner(),
                              MinMaxAll::all(),
                              false,
                              false,
                              false,
                              1.0,
                              ReducedParasiticType::none,
                              false,
                              false,
                              factor,
                              sta_);
  if (ret){
    map.clear();
    buildSpefDelayMap(map);
  }

  return ret;
}

void WireBudgeting::getNewWireLength(Vertex* pred,
                                     Vertex* succ,
                                     uint64& max_length,
                                     Delay& bud_rate)
{
  Name pred_name = pred->name(sta_->sdcNetwork());
  Name succ_name = succ->name(sta_->sdcNetwork());

  Vertex* fanout = getRealFanoutVertex(WirePair(pred_name, succ_name));
  Index pred_index = getIndex(pred);
  Index succ_index = getIndex(fanout);

  uint64 origin_length = getTreeDist(pred, succ);
  Delay origin_delay = getLowBound(pred, fanout);
  Delay max_delay = getHighBound(pred, fanout);

  Delay new_delay = arrival_map_[succ_index] - arrival_map_[pred_index];

  bud_rate = std::max(LRL_H_ZERO,
                      (new_delay - origin_delay) / (max_delay - origin_delay));
  bud_rate = std::min(1.0f, bud_rate);
  uint64 aug_length = round(origin_length * bud_rate);

  debugPrint(logger_,
             utl::LRL,
             "LRL",
             3,
             "{} -> {}, origin = {}, aug = {}",
             pred->name(sta_->sdcNetwork()),
             succ->name(sta_->sdcNetwork()),
             origin_length,
             aug_length);
  max_length = origin_length + aug_length;
  bud_rate = bud_rate + 1.0f;
}

bool WireBudgeting::hasWire(const WirePair& wire)
{
  return opt_wires_.find(wire) != opt_wires_.end();
}

////////////////////////////////////////////////////////
//
//  class WireBudgetingFake
//
////////////////////////////////////////////////////////

void WireBudgetingFake::initAlgorithm()
{
  VertexSet* ends = sta_->search()->endpoints();
  VertexSet failures;
  int cnt = 0;
  for (Vertex* vertex : *ends) {
    Slack slack = sta_->vertexSlack(vertex, MinMax::max());
    if (!fuzzyInf(slack)) {
      failures.insert(vertex);
      cnt++;
    }
  }
  logger_->info(utl::LRL, 1403, "detected {} Budgeting Ends", cnt);
  buildIndexMap(failures);
}

void WireBudgetingFake::getNewWireLength(Vertex* pred,
                                         Vertex* succ,
                                         uint64& max_length,
                                         Delay& bud_rate)
{
  uint64 origin_length = getTreeDist(pred, succ);
  uint64 aug_length = LRL_S_ZERO;
  Name pred_name = pred->name(sta_->sdcNetwork());
  Name succ_name = succ->name(sta_->sdcNetwork());
  Vertex* fanout = getRealFanoutVertex(WirePair(pred_name, succ_name));

  Slack slack = sta_->vertexSlack(fanout, MinMax::max());
  PathRef path = sta_->vertexWorstSlackPath(fanout, max_);
  const Slack& period = path.clock(sta_)->period();
  if (slack >= factor_ * period) {
    aug_length = origin_length;
  }

  debugPrint(logger_,
             utl::LRL,
             "LRL",
             3,
             "{} -> {}, origin = {}, aug = {}",
             pred->name(sta_->sdcNetwork()),
             succ->name(sta_->sdcNetwork()),
             origin_length,
             aug_length);
  max_length = origin_length + aug_length;
  if (max_length == 0) {
    bud_rate = LRL_H_INFINITY;
  } else
    bud_rate = max_length / origin_length;
}

void WireBudgetingFake::addConstraintEdge(Vertex* pred, Vertex* succ)
{
}

void WireBudgetingFake::addConstraintPrimeOut(Vertex* po)
{
}
void WireBudgetingFake::addConstraintPrimeIn(Vertex* pi)
{
}

////////////////////////////////////////////////////////
//
//  class WireBudgetingRC
//
////////////////////////////////////////////////////////

void WireBudgetingRC::initAlgorithm()
{
  VertexSet* ends = sta_->search()->endpoints();
  VertexSet failures;
  int cnt = 0;
  for (Vertex* vertex : *ends) {
    Slack slack = sta_->vertexSlack(vertex, MinMax::max());
    if (!fuzzyInf(slack)) {
      failures.insert(vertex);
      cnt++;
    }
  }
  logger_->info(utl::LRL, 1404, "detected {} Budgeting Ends", cnt);
  buildIndexMap(failures);
}

Delay WireBudgetingRC::costSlope(Vertex* pred, Vertex* succ)
{
  return cost2Delay(100);
}

Delay WireBudgetingRC::getLowBound(Vertex* pred, Vertex* succ)
{
  return getCellDelay(succ);
}

Delay WireBudgetingRC::getHighBound(Vertex* pred, Vertex* succ)
{
  double length = dbuToMeters(getTreeDist(pred, succ));
  return getLowBound(pred, succ) + getDrvrSlope() * length * length;
}

Delay WireBudgetingRC::getDrvrSlope()
{
  if (fuzzyEqual(wire_slope_, LRL_H_ZERO)) {
    const double& wire_cap
        = openroad_->getResizer()->wireSignalCapacitance(sta_->cmdCorner());
    const double& wire_res
        = openroad_->getResizer()->wireSignalResistance(sta_->cmdCorner());
    wire_slope_ = wire_cap * wire_res;
    logger_->info(
        utl::LRL, 1405, "rc slope is {}", delayAsString(wire_slope_, sta_, 3));
  }
  return wire_slope_;
}

Delay WireBudgetingRC::getCellDelay(Vertex* vertex)
{
  Name vertex_name = vertex->name(sta_->sdcNetwork());
  if (!delay_map_.hasKey(vertex_name)) {
    Delay load_delay = LRL_H_ZERO;
    PathRef path = sta_->vertexWorstSlackPath(vertex, max_);
    if (!path.vertex(sta_))
      return LRL_H_ZERO;
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
  return delay_map_.at(vertex_name);
}

}  // namespace lrl