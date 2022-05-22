#include "lrl/Laurel.hh"

namespace lrl {

using namespace sta;

////////////////////////////////////////////////////////
//
//  class WireBudgeting
//
////////////////////////////////////////////////////////s

void WireBudgeting::initAlgorithm()
{
  VertexSet* ends = sta_->search()->endpoints();
  VertexSet failures;

  logger_->info(utl::LRL, 1402, "Budgeting Ends:");
  for (Vertex* vertex : *ends) {
    Slack slack = sta_->vertexSlack(vertex, MinMax::max());
    if (!fuzzyInf(slack)) {
      failures.insert(vertex);
      logger_->info(utl::LRL,
                    1403,
                    "   {} : {}",
                    delayAsString(slack, sta_, 3),
                    vertex->name(sta_->sdcNetwork()));
    }
  }
  buildIndexMap(failures);
}

Delay WireBudgeting::costSlope(Vertex* pred, Vertex* succ)
{
  uint64 wire_length = getTreeDist(pred, succ);
  LibertyPort* drvr_port = sta_->network()->libertyPort(pred->pin());
  LibertyPort* load_port = sta_->network()->libertyPort(succ->pin());
  Delay min_delay = LRL_H_ZERO;
  Delay max_delay = LRL_H_ZERO;
  Slew slew = LRL_H_ZERO;  // unused
  cellWireDelay(
      drvr_port, load_port, dbuToMeters(wire_length), min_delay, slew);
  cellWireDelay(
      drvr_port, load_port, dbuToMeters(2 * wire_length), max_delay, slew);
  // printf("len: %lld, max delay: %s , min delay: %s, cell delay: %s\n",
  //        wire_length,
  //        delayAsString(max_delay, sta_, 3),
  //        delayAsString(min_delay, sta_, 3),
  //        delayAsString(getDelay(pred), sta_, 3));
  if (max_delay == min_delay)
    return LRL_S_INT_INF;
  return wire_length / (max_delay - min_delay) / LRL_H_STA_SCALING
         / LRL_H_SCALING;
}

Delay WireBudgeting::getLowBound(Vertex* pred, Vertex* succ)
{
  uint64 wire_length = getTreeDist(pred, succ);
  LibertyPort* drvr_port = sta_->network()->libertyPort(pred->pin());
  LibertyPort* load_port = sta_->network()->libertyPort(succ->pin());
  Delay delay = LRL_H_ZERO;
  Slew slew = LRL_H_ZERO;  // unused
  cellWireDelay(drvr_port, load_port, dbuToMeters(wire_length), delay, slew);
  return delay;
}

Delay WireBudgeting::getHighBound(Vertex* pred, Vertex* succ)
{
  uint64 wire_length = getTreeDist(pred, succ);
  LibertyPort* drvr_port = sta_->network()->libertyPort(pred->pin());
  LibertyPort* load_port = sta_->network()->libertyPort(succ->pin());
  Delay delay = LRL_H_ZERO;
  Slew slew = LRL_H_ZERO;  // unused
  cellWireDelay(
      drvr_port, load_port, dbuToMeters(2 * wire_length), delay, slew);
  return delay;
}

void WireBudgeting::addConstraintEdge(Vertex* pred, Vertex* succ)
{
  // Delay delay = getDelay(succ);
  // Index pred_index = getIndex(pred);
  // Index succ_index = getIndex(succ);
  // Cost capacity = delay2Cost(costSlope(pred, succ));
  // Delay low_bound = getLowBound(pred, succ);
  // Delay high_bound = getHighBound(pred, succ);

  Name pred_name = pred->name(sta_->sdcNetwork());
  Name succ_name = succ->name(sta_->sdcNetwork());
  WirePair wire = WirePair(pred_name, succ_name);
  opt_wires_.insert(wire);

  // addConstraintBase(pred_index, succ_index, capacity, -delay2Cost(delay));
  // addConstraintBase(
  //     pred_index, succ_index, LRL_S_INT_INF, -delay2Cost(low_bound));
  // addConstraintBase(
  //     succ_index, pred_index, LRL_S_INT_INF, delay2Cost(high_bound));
}

uint64 WireBudgeting::getTreeDist(Vertex* drvr, Vertex* dst)
{
  uint64 wire_length = LRL_S_ZERO;
  SteinerTree* tree = makeSteinerTree(drvr->pin(),
                                      true,
                                      max_steiner_pin_count_,
                                      stt_builder_,
                                      sta_->getDbNetwork(),
                                      logger_);
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

uint64 WireBudgeting::getNewWireLength(Vertex* pred,
                                       Vertex* succ,
                                       uint64& max_length,
                                       Delay& bud_rate)
{
  Index pred_index = getIndex(pred);
  Index succ_index = getIndex(succ);
  // LibertyPort* drvr_port = sta_->network()->libertyPort(pred->pin());
  // LibertyPort* load_port = sta_->network()->libertyPort(succ->pin());
  uint64 origin_length = getTreeDist(pred, succ);
  // Delay origin_delay = LRL_H_ZERO;
  // Slew slew = LRL_H_ZERO;  // unused

  // cellWireDelay(drvr_port, load_port, origin_length, origin_delay, slew);
  // Delay new_delay = arrival_map_[succ_index] - arrival_map_[pred_index];

  // uint64 aug_length
  //     = round((new_delay - origin_length) * costSlope(pred, succ));
  uint64 aug_length = origin_length;  // debug

  debugPrint(logger_,
             utl::LRL,
             "LRL",
             3,
             "{} -> {}, origin = {}, aug = {}",
             getNodeVertexName(pred_index),
             getNodeVertexName(succ_index),
             origin_length,
             aug_length);
  max_length = origin_length + aug_length;
  if (max_length == 0) {
    bud_rate = LRL_H_INFINITY;
  } else
    bud_rate = max_length / origin_length;
}

}  // namespace lrl