#include "rsz/IncrTdpDataWriter.hh"

#include "sta/Sdc.hh"
#include "sta/PortDirection.hh"
#include "sta/Graph.hh"
#include "sta/Network.hh"
#include "sta/Liberty.hh"
#include "sta/TimingRole.hh"
#include "sta/Corner.hh"
#include "sta/MinMax.hh"
#include "sta/TimingArc.hh"
#include "sta/Fuzzy.hh"
#include "sta/DcalcAnalysisPt.hh"
#include "sta/TableModel.hh"
#include "sta/GraphDelayCalc.hh"
#include "sta/PathRef.hh"

#include "ord/OpenRoad.hh"
#include "db_sta/dbSta.hh"
#include "rsz/Resizer.hh"

#include <iostream>
#include <fstream>

#include "incr_tdp_data.pb.h"

namespace rsz {

using namespace sta;

////////////////////////////////////////////////////////
//
//  Wrapper for calling STA
//
////////////////////////////////////////////////////////

typedef std::pair<double, double> Tangent;

class StaAdapter {
  struct StaTable2 : public Table2
  {
    // whether the `axis` mode
    //    ordinary mode:    v1 => in_slew, v2 => load_cap
    //    check mode:       v1 => from_slew, v2 => to_slew
    double selectValue(TableAxis* axis, double v1, double v2) const
    {
      TableAxisVariable var = axis->variable();
      if (var == TableAxisVariable::input_transition_time
          || var == TableAxisVariable::input_net_transition)
        return v1; // in_slew
      if (var == TableAxisVariable::total_output_net_capacitance)
        return v2; // load_cap
      if (var == TableAxisVariable::related_pin_transition)
        return v1; // from_slew
      if (var == TableAxisVariable::constrained_pin_transition)
        return v2; // to_slew
      std::cerr << "<!LRL-ERR> unsupported table axes" << std::endl;
      return 0;
    }

    Tangent tangentBase(double v1, double v2, bool revise) const
    {
      // TODO: needs confirm
      size_t id1 = axis1_->findAxisIndex(v1);
      size_t id2 = axis2_->findAxisIndex(v2);
      double dx1 = axis1_->axisValue(id1 + 1) - axis1_->axisValue(id1);
      double dx2 = axis2_->axisValue(id2 + 1) - axis2_->axisValue(id2);
    
      double y00 = tableValue(id1, id2);
      double dy1 = tableValue(id1 + 1, id2) - y00;
      double dy2 = tableValue(id1, id2 + 1) - y00;

      double tangent1 = dy1 / dx1;
      double tangent2 = dy2 / dx2;

      if (revise) return {tangent2, tangent1};
      return {tangent1, tangent2};
    }

    Tangent tangent(double k1, double k2) const
    {
      double v1 = selectValue(axis1_, k1, k2);
      double v2 = selectValue(axis2_, k1, k2);
      return tangentBase(v1, v2, !fuzzyEqual(v1, k1));
    }
  };

  struct StaTableModel2 : public TableModel
  {
    // if is_check table (mode):
    //    input:  k1 => from_slew, k2 => to_slew
    //    return value: first is `from_slew arg`, second is `to_slew arg`
    // else:
    //    input:  k1 => in_slew , k2 => load_cap
    //    return value: first is `slew arg`, second is `cap arg`
    Tangent tangent(double v1, double v2) const
    {
      StaTable2* table = static_cast<StaTable2*>(table_);
      return table->tangent(v1, v2);
    }
  };

  // this is an wrapper for `CheckTableModel` to access its protected `model_`
  // There's no full definition of this class so you cannot do dynamic_cast
  // ONLY `sta::CheckTableModel` is able to do static_cast with this wrapper!!
  struct StaCheckTableModel2 : public CheckTableModel
  {
    const TableModel* model() const { return model_; }
  };

 public:
  StaAdapter() = delete;
  StaAdapter(Sta* s): sta_(s) {}
  virtual ~StaAdapter() = default;

  // api wrapper to get sta informations
  inline Graph* graph() const { return sta_->graph(); }
  inline Network* network() const { return sta_->network(); }
  inline Sdc* sdc() const { return sta_->sdc(); }
  inline const MinMax* max() const { return MinMax::max(); }
  inline DcalcAnalysisPt* ap() const { return sta_->cmdCorner()->findDcalcAnalysisPt(max()); }
  inline int apIndex() const { return ap()->index(); }
  inline Instance* inst(Vertex* v) const { return network()->instance(v->pin()); }
  inline LibertyPort* port(Vertex* v) const { return network()->libertyPort(v->pin()); }
  inline PortDirection* dir(Vertex* v) const { return network()->direction(v->pin()); }
  inline Corner* corner() const { return sta_->cmdCorner(); }
  inline Net* net(Vertex* v) const { return network()->net(v->pin()); }

  inline bool isDrvr(Vertex* v) const { return network()->isDriver(v->pin()); }
  inline bool isClock(Vertex* v) const { return sta_->isClock(v->pin()); }
  inline bool isPort(Vertex* v) const {return network()->isTopLevelPort(v->pin()); }
  bool isRegister(Vertex *v)
  {
    LibertyPort *port = network()->libertyPort(v->pin());
    if (port) {
      LibertyCell *cell = port->libertyCell();
      return cell && cell->hasSequentials();
    }
    return false;
  }

  inline RiseFall* fromRf(TimingArc* arc) const { return arc->fromTrans()->asRiseFall(); }
  inline RiseFall* toRf(TimingArc* arc) const { return arc->toTrans()->asRiseFall(); }
  inline Vertex* from(Edge* e) const { return e->from(graph()); }
  inline Vertex* to(Edge* e) const { return e->to(graph()); }

  inline const char* name(Vertex* v) const { return v->name(network()); }
  inline const char* instName(Vertex* v) const { return network()->name(inst(v)); }
  inline const char* fromName(Edge* e) const { return name(from(e)); }
  inline const char* toName(Edge* e) const { return name(to(e)); }
  inline const char* fromName(TimingArc* arc) const { return arc->from()->name(); }
  inline const char* toName(TimingArc* arc) const { return arc->to()->name(); }
  inline const char* portName(Vertex* v) const { return network()->libertyPort(v->pin())->name(); }

  inline double wireRes(Resizer* rsz) const { return rsz->wireSignalResistance(corner()); }
  inline double wireCap(Resizer* rsz) const { return rsz->wireSignalCapacitance(corner()); }
  inline double slew(Vertex* v, const RiseFall* rf) const { return graph()->slew(v, rf, apIndex()); }
  inline double loadCap(Edge* e, TimingArc* arc) const 
  { return sta_->graphDelayCalc()->loadCap(to(e)->pin(), toRf(arc), ap()); }
  inline double loadCap(Vertex* v) const
  { return sta_->graphDelayCalc()->loadCap(v->pin(), ap()); }
  
  inline Delay arcDelay(Edge* e, TimingArc* arc) const { return graph()->arcDelay(e, arc, apIndex()); }
  inline Slack maxSlack(Vertex* v) const { return sta_->vertexSlack(v, max()); }
  inline Slack maxFromSlack(Edge* e) const { return maxSlack(from(e)); }
  inline Slack maxToSlack(Edge* e) const { return maxSlack(to(e)); }
  inline Arrival maxFromArrival(Edge* e) const
  { if (sta_->isClock(from(e)->pin())) return maxClkEdgeFromArrival(e); return maxArrival(from(e)); }
  inline Arrival maxToArrival(Edge* e) const { return maxArrival(to(e)); }
  inline Arrival maxClkEdgeFromArrival(Edge* e) const
  {
    Arrival arr = 0;
    Vertex* pv = from(e);
    VertexPathIterator path_iter(to(e), sta_);
    while (path_iter.hasNext()) {
      Path* path = path_iter.next();
      TimingArc* arc = nullptr;
      PathRef p;
      path->prevPath(sta_, p, arc);
      if (p.vertex(sta_) == pv) {
        Arrival path_arrival = p.arrival(sta_);
        if (fuzzyLess(arr, path_arrival))
          arr = path_arrival;
      }
    }
    return arr;
  }
  Arrival maxArrival(Vertex* v) const
  {
    Arrival arr = 0;
    VertexPathIterator path_iter(v, sta_);
    while (path_iter.hasNext()) {
      Arrival const& path_arrival = path_iter.next()->arrival(sta_);
      if (fuzzyLess(arr, path_arrival))
        arr = path_arrival;
    }
    return arr;
  }

  double capLimit(LibertyPort* out_port)
  {
    bool exist = false;
    float limit = 0;
    out_port->capacitanceLimit(max(), limit, exist);
    if (!exist)
      return 0;
    return limit;
  }

  double capLimit(Vertex* v)
  {
    float limit = INF;
    bool exist = false;
    sdc()->capacitanceLimit(v->pin(), max(), limit, exist);
    if (!exist)
      return INF;
    return limit;
  }

  void tangent(Edge* e, TimingArc* arc, Tangent& delay, Tangent& transition) const
  {
    double in_slew = slew(from(e), fromRf(arc));
    GateTableModel* model = dynamic_cast<GateTableModel*>(arc->model());
    const StaTableModel2* d_table = static_cast<const StaTableModel2*>(model->delayModel());
    const StaTableModel2* t_table = static_cast<const StaTableModel2*>(model->slewModel());
    double load_cap = loadCap(e, arc);
    delay = std::move(d_table->tangent(in_slew, load_cap));
    transition = std::move(t_table->tangent(in_slew, load_cap));
  }

  void tangentSetup(Edge* e, TimingArc* arc, Tangent& setup) const
  {
    double in_slew = slew(from(e), fromRf(arc));
    CheckTableModel* model = dynamic_cast<CheckTableModel*>(arc->model());
    StaCheckTableModel2* wrap = static_cast<StaCheckTableModel2*>(model);
    const StaTableModel2* s_table = static_cast<const StaTableModel2*>(wrap->model());
    double to_slew = slew(to(e), toRf(arc));
    setup = std::move(s_table->tangent(in_slew, to_slew));
  }

 protected:
  Sta* sta_;
};

////////////////////////////////////////////////////////
//
//  Filler to fill data into pb structure
//
////////////////////////////////////////////////////////

class IncrTdpDataFiller {
  lrl_tdp::IncrTdpData* tdp_;
  StaAdapter ad_;

  struct TimingArcInfo {
    Delay arc_delay = -INF;
    TimingArc* arc = nullptr;
    Edge* edge = nullptr;
    inline bool valid() const { return arc && edge; }
  } worst_;

  void fillArc(Slack from, Slack to)
  {
    Edge* e = worst_.edge;
    TimingArc* arc = worst_.arc;

    lrl_tdp::Arc* pb_arc = tdp_->add_arcs();
    pb_arc->set_instance(ad_.instName(ad_.to(e)));
    pb_arc->set_pin_start(ad_.fromName(arc));
    pb_arc->set_pin_end(ad_.toName(arc));
    pb_arc->set_input_slack(from);
    pb_arc->set_output_slack(to);
    pb_arc->set_input_arr(ad_.maxFromArrival(e));
    pb_arc->set_output_arr(ad_.maxToArrival(e));
    pb_arc->set_arc_delay(worst_.arc_delay);
    pb_arc->set_c_max(ad_.capLimit(arc->to()));
    pb_arc->set_c_pins(ad_.loadCap(e, arc));

    Tangent d = {0, 0};
    Tangent t = {0, 0};
    ad_.tangent(e, arc, d, t);

    pb_arc->add_delay_args(d.first);
    pb_arc->add_delay_args(d.second);
    pb_arc->add_transition_args(t.first);
    pb_arc->add_transition_args(t.second);

    pb_arc->set_input_slew(ad_.slew(e->from(ad_.graph()), arc->fromTrans()->asRiseFall()));
    pb_arc->set_output_slew(ad_.slew(e->to(ad_.graph()), arc->toTrans()->asRiseFall()));
  }

  void fillEdge(Vertex* v)
  {
    VertexInEdgeIterator iter(v, ad_.graph());
    while (iter.hasNext()) {
      Edge* e = iter.next();
      TimingArcSet* arc_set = e->timingArcSet();
      assert(!arc_set->role()->isTimingCheck());
      assert(!e->isWire());

      Slack from = ad_.maxFromSlack(e);
      Slack to = ad_.maxToSlack(e);
      // if(fuzzyInf(from) || fuzzyInf(to))
      //   return;

      TimingArcSetArcIterator arc_iter(arc_set);
      while(arc_iter.hasNext()) {
        TimingArc* arc = arc_iter.next();
        Delay arc_delay = ad_.arcDelay(e, arc);
        if(worst_.arc_delay < arc_delay)
          worst_ = {arc_delay, arc, e};
      }
  
      if(!worst_.valid())
        continue;

      fillArc(from, to);
      worst_ = std::move(TimingArcInfo()); // reset
    }
  }

  double worstArcSetupArg(Vertex* v) const
  {
    TimingArcInfo worst;
    VertexInEdgeIterator iter(v, ad_.graph());
    while (iter.hasNext()) {
      Edge* e = iter.next();
      TimingArcSet* arc_set = e->timingArcSet();
      if(e->isWire()) continue;
      assert(arc_set->role()->isTimingCheck());

      TimingArcSetArcIterator arc_iter(arc_set);
      while(arc_iter.hasNext()) {
        TimingArc* arc = arc_iter.next();
        Delay arc_delay = ad_.arcDelay(e, arc);
        if(worst.arc_delay < arc_delay)
          worst = {arc_delay, arc, e};
      }
    }
    if(worst.valid()) {
      Tangent setup = {0, 0};
      ad_.tangentSetup(worst.edge, worst.arc, setup);
      return setup.first; // output slew, means Q slew
    }
    return 0;
  }

  bool isEndPoint(Vertex* v) {
    VertexOutEdgeIterator next_iter(v, ad_.graph());
    size_t next_sz = 0;
    while (next_iter.hasNext()) { next_iter.next(); ++next_sz; }
    
    VertexInEdgeIterator pre_iter(v, ad_.graph());
    size_t pre_sz = 0;
    while (pre_iter.hasNext()) { pre_iter.next(); ++pre_sz; }

    return (!next_sz && pre_sz);
  }

  void fillSeqPin(Vertex* v)
  {
    bool is_clk = ad_.isClock(v);
    bool is_clk_gate = ad_.network()->libertyPort(v->pin())->libertyCell()->isClockGate();
    if (is_clk_gate && is_clk)
      return;
    if (!ad_.isClock(v) && !isEndPoint(v))
      return;
    Slack s = ad_.maxSlack(v);
    // if(fuzzyInf(s))
    //   return;

    PortDirection* dir = ad_.dir(v);
    lrl_tdp::SeqPin* pb_seq_pin = tdp_->add_seq_pins();
    pb_seq_pin->set_slack(s);
    pb_seq_pin->set_instance(ad_.instName(v));
    pb_seq_pin->set_pin(ad_.portName(v));

    if (is_clk && !is_clk_gate) {
      pb_seq_pin->set_is_start(dir->isAnyInput());
      pb_seq_pin->set_is_end(dir->isAnyOutput());

      if(pb_seq_pin->is_start())
        pb_seq_pin->set_c_pins(ad_.loadCap(v));

      if(pb_seq_pin->is_end())
        pb_seq_pin->set_setup_args(worstArcSetupArg(v));
    } else {
      pb_seq_pin->set_is_start(dir->isAnyOutput());
      pb_seq_pin->set_is_end(dir->isAnyInput());

      if(pb_seq_pin->is_start())
        pb_seq_pin->set_c_pins(ad_.loadCap(v));

      if(pb_seq_pin->is_end())
        pb_seq_pin->set_setup_args(worstArcSetupArg(v));
    }
    pb_seq_pin->set_arr(ad_.maxArrival(v));
    pb_seq_pin->set_slew(std::max(ad_.slew(v, RiseFall::rise()), ad_.slew(v, RiseFall::fall())));
  }


  void fillCell(Vertex* v)
  {
    LibertyPort *port = ad_.port(v);
    if(!port)
      return;
    
    LibertyCell* cell = port->libertyCell();
    if(!cell)
      return;
    
    if(cell->hasSequentials() || cell->isClockGate()) {
      if (ad_.isDrvr(v)) {
        VertexOutEdgeIterator iter(v, ad_.graph());
        if(!iter.hasNext())
          return;
      }
      fillSeqPin(v);
    }

    bool is_clk_gate = ad_.network()->libertyPort(v->pin())->libertyCell()->isClockGate();
    if(ad_.isDrvr(v) && !is_clk_gate)
      return fillEdge(v);
  }

  void fillPort(Vertex* v)
  {
    PortDirection* dir = ad_.dir(v);
    lrl_tdp::Port* pb_port = tdp_->add_ports();

    pb_port->set_port_name(ad_.name(v));
    pb_port->set_is_input(dir->isAnyInput());
    pb_port->set_is_output(dir->isAnyOutput());
    pb_port->set_c_max(ad_.capLimit(v));
    pb_port->set_c_pins(ad_.loadCap(v));
    pb_port->set_output_slack(ad_.maxSlack(v));
    pb_port->set_arr(ad_.maxArrival(v));
    pb_port->set_slew(std::max(ad_.slew(v, RiseFall::rise()), ad_.slew(v, RiseFall::fall())));
  }

public:
  IncrTdpDataFiller() = delete;
  IncrTdpDataFiller(lrl_tdp::IncrTdpData* t, Sta* s)
    : tdp_(t), ad_(s) {};
  virtual ~IncrTdpDataFiller() = default;

  void fillTdpData(Resizer* rsz)
  {
    VertexIterator iter(ad_.graph());
    while (iter.hasNext()) {
      Vertex* v = iter.next();
      if(ad_.isPort(v))
        fillPort(v);
      else
        fillCell(v);
    }
    // fill other variables
    tdp_->set_wire_res(ad_.wireRes(rsz));
    tdp_->set_wire_cap(ad_.wireCap(rsz));
  }
};

////////////////////////////////////////////////////////
//
//  class IncrTdpDataWriter
//
////////////////////////////////////////////////////////

bool IncrTdpDataWriter::serialize(std::string const& file_name)
{
  if(file_name.empty()) {
    std::cerr << "[LRL-TDP] no file_name input" << std::endl;
    return false;
  }

  Sta* sta = ord_->getSta();
  sta->checkCapacitanceLimitPreamble();
  sta->ensureClkNetwork();

  lrl_tdp::IncrTdpData tdp;
  IncrTdpDataFiller filler(&tdp, sta);
  filler.fillTdpData(ord_->getResizer());

  std::fstream output(file_name.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
  if(!tdp.SerializeToOstream(&output)) {
    std::cerr << "[LRL-TDP] fail to write out pb file" << std::endl;
    return false;
  }
  std::cout << "[LRL-TDP] write incr tdp data pb file to: " << file_name << std::endl;
  return true;
}

////////////////////////////////////////////////////////
//
//  DEBUG: deserialize
//
////////////////////////////////////////////////////////

const char* str(bool b) { return b ? "true" : "false"; }

void report(lrl_tdp::IncrTdpData const& tdp)
{
  std::cout << "<wire_cap>: " << tdp.wire_cap() << std::endl;
  std::cout << "<wire_res>: " << tdp.wire_res() << std::endl;
  // ports
  for (int i = 0; i < tdp.ports_size(); ++i) {
    auto& pb_port = tdp.ports(i);
    std::cout << "port_name: " << pb_port.port_name() << std::endl;
    std::cout << "is_input: " << str(pb_port.is_input()) << std::endl;
    std::cout << "  c_pins: " << pb_port.c_pins() << std::endl;
    std::cout << "  c_max: " << pb_port.c_max() << std::endl;
    std::cout << "is_output: " << str(pb_port.is_output()) << std::endl;
    std::cout << "  output_slack: " << pb_port.output_slack() << std::endl;
    std::cout << "arr: " << pb_port.arr() << std::endl;
    std::cout << "slew: " << pb_port.slew() << std::endl;
    std::cout << "----------------------" << std::endl;
  }
  // seq_pins
  for (int i = 0; i < tdp.seq_pins_size(); ++i) {
    auto& pb_seq_pin = tdp.seq_pins(i);
    std::cout << "seq_instance: " << pb_seq_pin.instance() << std::endl;
    std::cout << "seq_pin_name: " << pb_seq_pin.pin() << std::endl;
    std::cout << "slack: " << pb_seq_pin.slack() << std::endl;
    std::cout << "is_start: " << str(pb_seq_pin.is_start()) << std::endl;
    if (pb_seq_pin.is_start())
      std::cout << "  c_pins: " << pb_seq_pin.c_pins() << std::endl;
    std::cout << "is_end: " << str(pb_seq_pin.is_end()) << std::endl;
    if (pb_seq_pin.is_end())
      std::cout << "  setup_args: " << pb_seq_pin.setup_args() << std::endl;
    std::cout << "arr: " << pb_seq_pin.arr() << std::endl;
    std::cout << "slew: " << pb_seq_pin.slew() << std::endl;
    std::cout << "----------------------" << std::endl;
  }
  // arcs
  for (int i = 0; i < tdp.arcs_size(); ++i) {
    auto& pb_arc = tdp.arcs(i);
    std::cout << "  instance: " << pb_arc.instance() << std::endl;
    std::cout << "  pin_start: " << pb_arc.pin_start() << std::endl;
    std::cout << "  pin_end: " << pb_arc.pin_end() << std::endl;
    std::cout << "  input_slack: " << pb_arc.input_slack() << std::endl;
    std::cout << "  output_slack: " << pb_arc.output_slack() << std::endl;
    std::cout << "  input_arr: " << pb_arc.input_arr() << std::endl;
    std::cout << "  output_arr: " << pb_arc.output_arr() << std::endl;
    std::cout << "  input_slew: " << pb_arc.input_slew() << std::endl;
    std::cout << "  output_slew: " << pb_arc.output_slew() << std::endl;
    std::cout << "  arc_delay: " << pb_arc.arc_delay() << std::endl;
    std::cout << "  c_max: " << pb_arc.c_max() << std::endl;
    std::cout << "  c_pins: " << pb_arc.c_pins() << std::endl;
    std::cout << "  delay_args: " << std::endl;
    for (int i = 0; i<2; ++i)
      std::cout << "    a" << i << ": " << pb_arc.delay_args(i) << std::endl;
    std::cout << "  transition_args: " << std::endl;
    for (int i = 0; i<2; ++i)
      std::cout << "    a" << i << ": " << pb_arc.transition_args(i) << std::endl;
    std::cout << "----------------------" << std::endl;
  }
}

bool IncrTdpDataWriter::deserialize(std::string const& file)
{
  lrl_tdp::IncrTdpData tdp;
  std::fstream input(file.c_str(), std::ios::in | std::ios::binary);
  if(!tdp.ParseFromIstream(&input)) {
      std::cerr << "[LRL-TDP] Failed to parse from: " << file << std::endl;
      return false;
  }
  std::cout << "[LRL-TDP] read pb_file from:" << file << std::endl;
  report(tdp);
  return true;
}

} // namespace lrl
