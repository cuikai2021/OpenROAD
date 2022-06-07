#include "lrl/Laurel.hh"
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

void WireBudgeting::buildAllFaninEdge(VertexSeq fanins)
{
  for (auto vertex : fanins) {
    bool is_pi = true;
    buildIndex(vertex);
    Name vertex_name = vertex->name(sta_->sdcNetwork());
    name_vertex_map_.insert(vertex_name, vertex);
    pin_vertex_map_.insert(vertex->pin(), vertex);
    SearchPredNonReg2 pred(sta_);
    BfsBkwdIterator iter(BfsIndex::other, &pred, this);
    iter.enqueueAdjacentVertices(vertex);
    while (iter.hasNext()) {
      Vertex* fanin = iter.next();
      if (!fanin->isDriver(sta_->network())) {
        BfsBkwdIterator fanin_iter(BfsIndex::other, &pred, this);
        fanin_iter.enqueueAdjacentVertices(fanin);
        while (fanin_iter.hasNext()) {
          Vertex* pred = fanin_iter.next();
          if (pred->isDriver(sta_->network())) {
            buildIndex(pred);
            addConstraintEdge(pred, vertex);
            is_pi = false;
            Name pred_name = pred->name(sta_->sdcNetwork());
            Name succ_name = vertex->name(sta_->sdcNetwork());
            wire_fanout_map_.insert(WirePair(pred_name, succ_name), fanin);
          }
        }
      } else {
        buildIndex(fanin);
        addConstraintEdge(fanin, vertex);
        is_pi = false;
        Name pred_name = fanin->name(sta_->sdcNetwork());
        Name succ_name = vertex->name(sta_->sdcNetwork());
        wire_fanout_map_.insert(WirePair(pred_name, succ_name), vertex);
      }
    }
    if (is_pi)
      addConstraintPI(vertex);
  }
}

WirePair WireBudgeting::getRealWire(WirePair wire)
{
  if (!wire_fanout_map_.hasKey(wire))
    return wire;
  Vertex* fanout = wire_fanout_map_.findKey(wire);
  Name fanout_name = fanout->name(sta_->sdcNetwork());
  return WirePair(wire.first, fanout_name);
}

Vertex* WireBudgeting::getRealFanoutVertex(WirePair wire)
{
  if (!wire_fanout_map_.hasKey(wire))
    return findRealVertex(wire.second);
  return wire_fanout_map_.findKey(wire);
}

void WireBudgeting::buildALLEdgePO(VertexSet ends)
{
  for (auto node : vertex_index_map_) {
    Vertex* vertex = name_vertex_map_.findKey(node.first);
    if (ends.find(vertex) != ends.end())
      addConstraintPO(vertex);
  }
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

  // printf("%s  len: %lld, max delay: %s , min delay: %s\n",
  //        succ_name.c_str(),
  //        wire_length,
  //        delayAsString(max_delay, sta_, 3),
  //        delayAsString(min_delay, sta_, 3));
  if (max_delay == min_delay)
    return LRL_S_INT_INF;
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
  Delay delay = high_bound;

  Name pred_name = pred->name(sta_->sdcNetwork());
  Name succ_name = succ->name(sta_->sdcNetwork());
  WirePair wire = WirePair(pred_name, succ_name);
  opt_wires_.insert(getRealWire(wire));

  addConstraintBase(pred_index, succ_index, capacity, -delay2Cost(delay));
  addConstraintBase(
      pred_index, succ_index, LRL_S_INT_INF, -delay2Cost(low_bound));
  addConstraintBase(
      succ_index, pred_index, LRL_S_INT_INF, delay2Cost(high_bound));
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
  uint64 wire_length = LRL_S_ZERO;
  Vertex* load;
  if (dst->isDriver(sta_->network())) {
    Name drvr_name = drvr->name(sta_->sdcNetwork());
    Name dst_name = dst->name(sta_->sdcNetwork());
    WirePair wire(drvr_name, dst_name);
    load = getRealFanoutVertex(wire);
  } else {
    load = dst;
  }
  SteinerTree* tree = getTree(drvr);
  if (tree) {
    SteinerPt cur = tree->drvrPt(sta_->network());
    SteinerPt last = SteinerTree::null_pt;
    SteinerPt next = findNextPt(tree, cur, last);
    Point dst_pt = sta_->getDbNetwork()->location(load->pin());
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
    return Point::manhattanDistance(drvr_pt, dst_pt);
  }
  return wire_length;
}

SteinerPt WireBudgeting::findNextPt(SteinerTree* tree,
                                    SteinerPt drvr_pt,
                                    SteinerPt last)
{
  SteinerPt left = tree->left(drvr_pt);
  if (left == last)
    return tree->right(drvr_pt);
  return left;
}

sta::Map<Name, Delay> WireBudgeting::buildSpefDelayMap()
{
  sta_->findRequireds();
  sta::Map<Name, Delay> spef_delay_map;
  VertexIterator vertex_iter(sta_->graph());
  while (vertex_iter.hasNext()) {
    Vertex* vertex = vertex_iter.next();
    Delay delay = *sta_->graph()->arrivals(vertex);
    if (!fuzzyZero(delay)) {
      Name vertex_name = vertex->name(sta_->sdcNetwork());
      spef_delay_map.insert(vertex_name, delay);
    }
  }
  return spef_delay_map;
}

bool WireBudgeting::makeDelaySpefCore(sta::Map<Name, Delay>& map, double factor)
{
  const char* filename = options_->spefFileName();
  bool ret = sta_->readSpef(filename,
                            sta_->getDbNetwork()->topInstance(),
                            sta_->cmdCorner(),
                            MinMaxAll::all(),
                            false,
                            false,
                            false,
                            factor,
                            ReducedParasiticType::none,
                            false,
                            false);
  if (ret)
    map = buildSpefDelayMap();

  return ret;
}

void WireBudgeting::getNewWireLength(Vertex* pred,
                                     Vertex* succ,
                                     uint64& max_length,
                                     Delay& bud_rate)
{
  Index pred_index = getIndex(pred);
  Index succ_index = getIndex(succ);
  Name succ_name = succ->name(sta_->sdcNetwork());

  uint64 origin_length = getTreeDist(pred, succ);
  Delay origin_delay = LRL_H_ZERO;
  Delay max_delay = LRL_H_ZERO;
  if (spef_delay_.hasKey(succ_name))
    origin_delay = spef_delay_.at(succ_name);
  if (spef_delay_factor_.hasKey(succ_name))
    max_delay = spef_delay_factor_.at(succ_name);

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

bool WireBudgeting::hasWire(WirePair wire)
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
  Slack slack = sta_->vertexSlack(succ, MinMax::max());
  PathRef path = sta_->vertexWorstSlackPath(succ, max_);
  Slack period = path.clock(sta_)->period();
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
  Name pred_name = pred->name(sta_->sdcNetwork());
  Name succ_name = succ->name(sta_->sdcNetwork());
  name_vertex_map_.insert(pred_name, pred);
  name_vertex_map_.insert(succ_name, succ);
  pin_vertex_map_.insert(pred->pin(), pred);
  pin_vertex_map_.insert(succ->pin(), succ);
  WirePair wire = WirePair(pred_name, succ_name);
  opt_wires_.insert(getRealWire(wire));
}

void WireBudgetingFake::addConstraintPO(Vertex* po)
{
}
void WireBudgetingFake::addConstraintPI(Vertex* pi)
{
}

}  // namespace lrl