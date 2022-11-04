#include "gui/gui.h"
#include "ord/OpenRoad.hh"
#include "lrl/LaurelAdapter.hh"
#include "DrvrBfsBkwdIterator.hh"

namespace lrl {

using namespace sta;
using namespace rsz;

////////////////////////////////////////////////////////
//
//  class LaurelAdapter
//
////////////////////////////////////////////////////////

void LaurelAdapter::getAllRelatedEnds(VertexSet ends,
                                      bool ignore_clk,
                                      VertexSet& related_ends)
{
  VertexSet fanin_roots = findFaninRoots(ends);
  related_ends.insert(ends.begin(), ends.end());
  sta::SearchPredNonLatch2 pred(sta_);
  BfsFwdIterator iter(BfsIndex::other, &pred, this);
  for (Vertex* reg_out : fanin_roots)
    iter.enqueueAdjacentVertices(reg_out);

  while (iter.hasNext()) {
    Vertex* vertex = iter.next();
    if (isRegister(vertex)) {
      if (ignore_clk && sta_->isClock(vertex->pin()))
        continue;
      if (!sta::fuzzyInf(sta_->vertexSlack(vertex, MinMax::max())))
        related_ends.insert(vertex);
    } else
      iter.enqueueAdjacentVertices(vertex);
  }
}

Delay LaurelAdapter::getArrival(Vertex* vertex, MinMax const* min_max) const
{
  Delay ex_arrival = LRL_H_ZERO;
  if (min_max == MinMax::min())
    ex_arrival = LRL_H_INFINITY;
  VertexPathIterator path_iter(vertex, sta_);
  while (path_iter.hasNext()) {
    Path* path = path_iter.next();
    Delay arrival = path->arrival(sta_);
    if (((fuzzyLess(ex_arrival, arrival)) ^ (min_max == MinMax::min()))
        && !fuzzyInf(arrival))
      ex_arrival = arrival;
  }
  return ex_arrival;
}

void LaurelAdapter::parasiticPreamble(Resizer* rsz)
{
  Corners* cs = sta_->corners();
  int const cnt = cs->count();
  wire_signal_res_.resize(cnt);
  wire_signal_cap_.resize(cnt);
  wire_clk_res_.resize(cnt);
  wire_clk_cap_.resize(cnt);
  for (Corner* c : *cs) {
    int const& id = c->index();
    wire_signal_res_[id] = rsz->wireSignalResistance(c);
    wire_signal_cap_[id] = rsz->wireSignalCapacitance(c);
    wire_clk_res_[id] = rsz->wireClkResistance(c);
    wire_clk_cap_[id] = rsz->wireClkCapacitance(c);
  }
  parasitics_src_ = ParasiticsSrc::placement;
}

void LaurelAdapter::makeBufferInstance(const char* tag,
                                       Point const& location,
                                       LibertyCell* buffer_cell,
                                       Instance*& buffer)
{
  string buffer_name = makeUniqueInstName(tag);
  Instance* parent = db_network_->topInstance();
  buffer = makeInstance(buffer_cell,
                        buffer_name.c_str(),
                        parent);
  
  journalMakeBuffer(buffer);
  inserted_buffer_count_++;
  designAreaIncr(area(db_network_->cell(buffer_cell)));
  setLocation(buffer, location);
}

void LaurelAdapter::connectNetLoad(Pin* pin, Net* net)
{
  LibertyPort* port = network_->libertyPort(pin);
  Instance* inst = network_->instance(pin);
  sta_->disconnectPin(pin);
  sta_->connectPin(inst, port, net);
}

LaurelAdapter::ArcDelayResult
LaurelAdapter::predictGateDelay(Edge* e,
                                TimingArc* arc,
                                LibertyCell* cell,
                                DcalcAnalysisPt const* dcalc_ap)
{
  Vertex *drvr = e->to(graph_);
  Pin const* drvr_pin = drvr->pin();
  Instance *drvr_inst = network_->instance(drvr_pin);
  RiseFall const* drvr_rf = arc->toTrans()->asRiseFall();
  RiseFall const* load_rf = arc->fromTrans()->asRiseFall();

  // calculator
  ArcDelayCalc* calc = sta_->arcDelayCalc();

  // pvt
  Pvt const* pvt = sdc_->pvt(drvr_inst, dcalc_ap->constraintMinMax());
  if (pvt == nullptr)
    pvt = dcalc_ap->operatingConditions();
  
  // parasitic
  Parasitic *parasitic = calc->findParasitic(drvr_pin, drvr_rf, dcalc_ap);

  // related_out_cap
  LibertyPort const* related_out_port = e->timingArcSet()->relatedOut();
  Pin const* related_out_pin = related_out_port
                              ? network_->findPin(drvr_inst, related_out_port)
                              : nullptr;
  float related_out_cap = 0.0;
  if (related_out_pin) {
    Parasitic *related_out_parasitic = calc->findParasitic(related_out_pin, drvr_rf, dcalc_ap);
    related_out_cap = graph_delay_calc_->loadCap(related_out_pin,
                                                related_out_parasitic,
                                                drvr_rf,
                                                dcalc_ap);
  }
  
  // in_slew
  Vertex* load = e->from(graph_);
  Slew const in_slew = graph_->slew(load, load_rf, dcalc_ap->index());

  // load_cap
  float load_cap = graph_delay_calc_->loadCap(drvr_pin, parasitic, drvr_rf, dcalc_ap);

  // arc delay
  ArcDelayResult res = { LRL_H_ZERO, LRL_H_ZERO };
  calc->gateDelay(cell, arc,
				in_slew, load_cap, parasitic,
				related_out_cap, pvt, dcalc_ap,
				res.delay, res.slew);
  return res;
}

ArcDelay LaurelAdapter::findArcDelay(Edge* e, TimingArc* arc, DcalcAnalysisPt const* dcalc_ap)
{
  return graph_->arcDelay(e, arc, dcalc_ap->index());
}

////////////////////////////////////////////////////////
//
//  class LaurelBase : public LaurelAdapter
//
////////////////////////////////////////////////////////

LaurelBase::LaurelBase(OpenRoad* openroad, Options* options): options_(options)
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
}

////////////////////////////////////////////////////////
//
//  class LaurelCollector : public LaurelBase
//
////////////////////////////////////////////////////////

void LaurelCollector::iterateWiresBfs(VertexSet const& ends, VertexSet& pi)
{
  SearchPredNonReg2 search_pred(sta_);
  DrvrBfsBkwdIterator iter(BfsIndex::other, &search_pred, this);
  
  for (auto& end : ends)
    iter.enqueueAdjacentVertices(end);

  while (iter.hasNext()) {
    Vertex* v = iter.next();
    if (!sta_->isClock(v->pin()) && v->isDriver(network_) && !isRegister(v))
      iter.enqueueAdjacentVertices(v);
    if (isRegister(v) || network_->isTopLevelPort(v->pin()))
      pi.insert(v);
  }
}

}  // namespace lrl