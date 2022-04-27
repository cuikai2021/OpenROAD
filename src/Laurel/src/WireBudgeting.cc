#include "lrl/Laurel.hh"

namespace lrl {

using namespace sta;

////////////////////////////////////////////////////////
//
//  class WireBudgeting
//
////////////////////////////////////////////////////////

Delay WireBudgeting::costSlope(Vertex* pred, Vertex* succ)
{
  uint64 wire_length = getTreeDist(pred, succ);
  if (wire_length == LRL_S_ZERO)
    return INF;

  LibertyPort* drvr_port = sta_->network()->libertyPort(pred->pin());
  LibertyPort* load_port = sta_->network()->libertyPort(succ->pin());
  Delay min_delay = LRL_H_ZERO;
  Delay max_delay = LRL_H_ZERO;
  Slew slew = LRL_H_ZERO;  // unused
  cellWireDelay(drvr_port, load_port, wire_length, min_delay, slew);
  cellWireDelay(drvr_port, load_port, wire_length, max_delay, slew);
  return (max_delay - min_delay) / wire_length;
}

Delay WireBudgeting::getLowBound(Vertex* pred, Vertex* succ)
{
  return costSlope(pred, succ) * getTreeDist(pred, succ);
}

Delay WireBudgeting::getHighBound(Vertex* pred, Vertex* succ)
{
  return costSlope(pred, succ) * getTreeDist(pred, succ) * 2;
}

void WireBudgeting::addConstraintEdge(Vertex* pred, Vertex* succ)
{
  Delay delay = getDelay(succ);
  Index pred_index = getIndex(pred);
  Index succ_index = getIndex(succ);
  Cost capacity = delay2Cost(costSlope(pred, succ) * delay);
  Delay low_bound = getLowBound(pred, succ);    // diff
  Delay high_bound = getHighBound(pred, succ);  // diff
  addConstraintBase(pred_index, succ_index, capacity, -delay2Cost(delay));
  addConstraintBase(
      pred_index, succ_index, LRL_S_INT_INF, -delay2Cost(low_bound));
  addConstraintBase(
      succ_index, pred_index, LRL_S_INT_INF, delay2Cost(high_bound));
}

// uint64 WireBudgeting::manhattanDistance(Vertex* pred, Vertex* succ)
// {
//   Name pred_name = pred->name(sta_->sdcNetwork());
//   Name succ_name = succ->name(sta_->sdcNetwork());
//   WirePair wire_pair = WirePair(pred_name, succ_name);
//   Pin* fanout_pin = findFanoutPin(wire_pair);
//   if (fanout_pin) {
//     Point fanin_pt = sta_->getDbNetwork()->location(pred->pin());
//     Point fanout_pt = sta_->getDbNetwork()->location(fanout_pin);
//     return Point::manhattanDistance(fanin_pt, fanout_pt);
//   }
//   return LRL_S_ZERO;
// }

uint64 WireBudgeting::getTreeDist(Vertex* drvr, Vertex* dst)
{
  uint64 wire_length = LRL_S_ZERO;
  SteinerTree* tree = makeSteinerTree(drvr->pin(),
                                      true,
                                      max_steiner_pin_count_,
                                      stt_builder_,
                                      sta_->getDbNetwork(),
                                      logger_);
  SteinerPt cur = tree->drvrPt(sta_->network());
  SteinerPt last = SteinerTree::null_pt;
  SteinerPt next = findNextPt(tree, cur, last);
  Point dst_pt = sta_->getDbNetwork()->location(dst->pin());
  while (dst_pt != tree->location(cur)) {
    if (next == SteinerTree::null_pt)
      return LRL_S_ZERO;
    Point cur_pt = tree->location(cur);
    Point next_pt = tree->location(next);
    wire_length += Point::manhattanDistance(cur_pt, next_pt);
    last = cur;
    cur = next;
    next = findNextPt(tree, cur, last);
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

// double WireBudgeting::getDrvrWireLength(Vertex* drvr)
// {
//   const Corner* corner = sta_->cmdCorner();
//   Pin* drvr_pin = drvr->pin();
//   LibertyPort* drvr_port = network_->libertyPort(drvr_pin);
//   double drvr_r = drvr_port->driveResistance();
//   double wire_res = wireSignalResistance(corner);
//   printf("drvr_r %s", delayAsString(drvr_r, sta_, 3));
//   printf("wire_res %s", delayAsString(wire_res, sta_, 3));
//   return drvr_r / wire_res;
// }

uint64 WireBudgeting::getNewWireLength(Vertex* pred, Vertex* succ)
{
  Index pred_index = getIndex(pred);
  Index succ_index = getIndex(succ);
  LibertyPort* drvr_port = sta_->network()->libertyPort(pred->pin());
  LibertyPort* load_port = sta_->network()->libertyPort(succ->pin());
  uint64 origin_length = getTreeDist(pred, succ);
  Delay origin_delay = LRL_H_ZERO;
  Slew slew = LRL_H_ZERO;  // unused

  cellWireDelay(drvr_port, load_port, origin_length, origin_delay, slew);
  Delay new_delay = arrival_map_[succ_index] - arrival_map_[pred_index];

  uint64 aug_length
      = round((new_delay - origin_length) / costSlope(pred, succ));
  return origin_length + aug_length;
}

}  // namespace lrl