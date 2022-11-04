#include "lrl/WireBudgeting.hh"
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

void WireBudgeting::initWire(Vertex* pred, Vertex* succ, Edge* e)
{
  Wire wire(pred, succ);
  buildDelay(wire, e);
  Vertex* fanin = e->from(graph_);
  Wire real_wire(pred, fanin);
  opt_wires_.insert(real_wire);
  wire_fanout_.insert({real_wire, succ});
}

Vertex* WireBudgeting::getFanoutVertex(const Wire& wire)
{
  auto it = wire_fanout_.find(wire);
  if (it == wire_fanout_.end())
    it = wire_fanout_.insert({wire, wire.second}).first;
  return it->second;
}

Delay WireBudgeting::costSlope(Wire const& w)
{
  uint64 wire_length = getTreeDist(w);
  Vertex* succ = w.second;
  Delay min_delay = LRL_H_ZERO;
  Delay max_delay = LRL_H_ZERO;

  if (spef_delay_.hasKey(succ))
    min_delay = spef_delay_.at(succ);
  if (spef_delay_factor_.hasKey(succ))
    max_delay = spef_delay_factor_.at(succ);

  debugPrint(logger_,
             utl::LRL,
             "LRL",
             3,
             "{}  len: {}, max delay: {} , min delay: {}",
             succ->name(sdc_network_),
             wire_length,
             delayAsString(max_delay, sta_, 3),
             delayAsString(min_delay, sta_, 3));
  if (fuzzyZero(max_delay) || fuzzyLessEqual(max_delay, min_delay))
    return LRL_H_INFINITY;
  return wire_length / (max_delay - min_delay) / LRL_H_STA_SCALING
         / LRL_H_SCALING / LRL_H_SCALING / LRL_H_SCALING;
}

Delay WireBudgeting::getLowBound(Wire const& w)
{
  Delay delay = LRL_H_ZERO;
  Vertex* succ = w.second;
  if (spef_delay_.hasKey(succ))
    delay = spef_delay_.at(succ);
  return delay;
}

Delay WireBudgeting::getHighBound(Wire const& w)
{
  Delay delay = LRL_H_ZERO;
  Vertex* succ = w.second;
  if (spef_delay_.hasKey(succ))
    delay = spef_delay_factor_.at(succ);
  return delay;
}

void WireBudgeting::addConstraintEdge(Wire const& w)
{
  Index pred = index_graph_[w.first];
  Index succ = index_graph_[w.second];
  Cost capacity = delay2Cost(costSlope(w));
  Delay low_bound = getLowBound(w);
  Delay high_bound = getHighBound(w);
  Delay& delay = high_bound;

  solver_.addConstraint({pred, succ, capacity, -delay2Cost(delay)});
  solver_.addConstraint({pred, succ, MCFSolver::inf, -delay2Cost(low_bound)});
  // addConstraintBase(
  //     succ_index, pred_index, MCFSolver::inf, delay2Cost(high_bound));
}

SteinerTree* WireBudgeting::getTree(Vertex* drvr)
{
  Pin* pin = drvr->pin();
  auto it = drvr_cache_.find(pin);
  if(it == drvr_cache_.end()) {
    SteinerTree* tree = makeSteinerTree(pin,
                                        true,
                                        max_steiner_pin_count_,
                                        stt_builder_,
                                        sta_->getDbNetwork(),
                                        logger_);
    it = drvr_cache_.insert({pin, {drvr, tree}}).first;
  }
  return it->second.tree;
}

Vertex* WireBudgeting::getVertex(Pin* pin) const
{
  auto it = drvr_cache_.find(pin);
  if (it == drvr_cache_.end())
    return nullptr;
  return it->second.vertex;
}

void WireBudgeting::deleteAllTrees()
{
  for (auto& d : drvr_cache_)
    delete d.second.tree;
}

Vertex* WireBudgeting::getFaninVertex(Wire const& wire) const
{
  Vertex* pred = wire.first;
  Vertex* succ = wire.second;
  if (!succ->isDriver(sta_->network()))
    return succ;
  VertexOutEdgeIterator pred_edge_iter(pred, sta_->graph());
  while (pred_edge_iter.hasNext()) {
    Edge* pred_edge = pred_edge_iter.next();
    Vertex* fanout = pred_edge->to(sta_->graph());
    VertexInEdgeIterator succ_edge_iter(succ, sta_->graph());
    while (succ_edge_iter.hasNext()) {
      Edge* succ_edge = succ_edge_iter.next();
      Vertex* fanin = succ_edge->from(sta_->graph());
      if (fanin == fanout)
        return fanin;
    }
  }
  return nullptr;
}

uint64 WireBudgeting::getTreeDist(Wire const& w)
{
  Vertex* drvr = w.first;
  Vertex* dst = getFaninVertex(w);
  Wire real(drvr, dst);
  auto it = real_wire_distance_.find(real);
  if (it == real_wire_distance_.end()) {
    uint64 wire_length = MCFSolver::zero;
    Vertex* drvr = real.first;
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
    if (wire_length == MCFSolver::zero) {
      // if pin count < 2, it would not create a steiner tree
      // use manhattan distance directly
      Point drvr_pt = sta_->getDbNetwork()->location(drvr->pin());
      Point dst_pt = sta_->getDbNetwork()->location(dst->pin());
      wire_length = Point::manhattanDistance(drvr_pt, dst_pt);
    }
    it = real_wire_distance_.insert({real, wire_length}).first;

  }
  return it->second;
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

Delay WireBudgeting::getCellDelay(Vertex* vertex)
{
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
    }
  }
  return load_delay;
}

void WireBudgeting::buildSpefDelayMap(VertexDelayMap& spef_delay_map)
{
  sta_->findRequireds();
  VertexIterator vertex_iter(sta_->graph());
  while (vertex_iter.hasNext()) {
    Vertex* vertex = vertex_iter.next();
    if (!vertex->isRoot() && vertex->isDriver(sta_->network())) {
      Delay delay = getCellDelay(vertex);
      spef_delay_map.insert(vertex, delay);
    }
  }
}

bool WireBudgeting::makeDelaySpefCore(VertexDelayMap& map,
                                      double factor)
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
  if (ret) {
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
  Vertex* fanout = getFanoutVertex({pred, succ});
  Wire w(pred, fanout);

  uint64 origin_length = getTreeDist(w);
  Delay origin_delay = getLowBound(w);
  Delay max_delay = getHighBound(w);

  Delay new_delay = index_graph_.result(fanout)
                    - index_graph_.result(pred);

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

bool WireBudgeting::hasWire(const Wire& wire)
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
  Vertex* fanout = getFanoutVertex({pred, succ});
  Wire w(pred, fanout);
  uint64 origin_length = getTreeDist(w);
  uint64 aug_length = MCFSolver::zero;

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

void WireBudgetingFake::addConstraintEdge(Wire const& w)
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

Delay WireBudgetingRC::costSlope(Wire const& w)
{
  return cost2Delay(100);
}

Delay WireBudgetingRC::getLowBound(Wire const& w)
{
  return wire_.delay(w);
}

Delay WireBudgetingRC::getHighBound(Wire const& w)
{
  double length = dbuToMeters(getTreeDist(w));
  return getLowBound(w) + getDrvrSlope() * length * length;
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

}  // namespace lrl