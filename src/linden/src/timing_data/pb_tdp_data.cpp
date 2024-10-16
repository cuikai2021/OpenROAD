#include "pb_tdp_data.hpp"
#include <spdlog/spdlog.h>
#include "envflag/envflag.hpp"
#include "incr_tdp_data.pb.h"
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
IMPORT_BOOL_FLAG_WITH_DEFAULT(LINDEN_IN_REGRESSION);
namespace rsz {

using namespace std;
using namespace sta;

class StaAdapter {
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

  inline RiseFall* toRf(TimingArc* arc) const { return arc->toEdge()->asRiseFall(); }
  inline Vertex* from(Edge* e) const { return e->from(graph()); }
  inline Vertex* to(Edge* e) const { return e->to(graph()); }

  inline const char* name(Vertex* v) const { return v->name(network()); }
  inline const char* instName(Vertex* v) const { return network()->name(inst(v)); }
  inline const char* fromName(Edge* e) const { return name(from(e)); }
  inline const char* toName(Edge* e) const { return name(to(e)); }
  inline const char* fromName(TimingArc* arc) const { return arc->from()->name(); }
  inline const char* toName(TimingArc* arc) const { return arc->to()->name(); }
  inline const char* portName(Vertex* v) const { return network()->libertyPort(v->pin())->name(); }
  inline LibertyCell* cell(Vertex* v) const { return network()->libertyCell(network()->instance(v->pin())); }

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
    if (sta_->isIdealClock(pv->pin())) {
      /*
        In the two stages place and place_opt, since CTS has not been done yet,
        timing of CK pin is invalid at this time.
        To avoid errors caused by the value of CK being passed down,
        it is set to 0
        and the same is true for the fn maxArrival
      */
      return arr;
    }
    VertexPathIterator path_iter(to(e), sta_);
    while (path_iter.hasNext()) {
      Path* path = path_iter.next();
      TimingArc* arc = nullptr;
      PathRef p;
      path->prevPath(sta_, p, arc);
      if (p.vertex(sta_) == pv) {
        Arrival path_arrival = p.arrival(sta_);
        if (fuzzyLess(arr, path_arrival)) {
          arr = path_arrival;
        }
      }
    }
    return arr;
  }
  Arrival maxArrival(Vertex* v) const
  {
    Arrival arr = 0;
    if (sta_->isClock(v->pin()) && sta_->isIdealClock(v->pin())) {
      // this is the same as the fn maxClkEdgeFromArrival
      return arr;
    }
    VertexPathIterator path_iter(v,sta_);
    while (path_iter.hasNext()) {
      PathVertex* path=path_iter.next();
      Arrival const& path_arrival = path->arrival(sta_);
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

protected:
  Sta* sta_;
};

class IncrTdpDataFiller {
  lrl_tdp::IncrTdpData* tdp_;
  StaAdapter ad_;

  struct TimingArcInfo {
    Delay arc_delay = -INF;
    TimingArc* arc = nullptr;
    Edge* edge = nullptr;
    inline bool valid() const { return arc && edge; }
  };

  pair<lrl_tdp::Arg, lrl_tdp::Arg> gradients(ArcDelay in_slew,
                                             double load_cap,
                                             const TableModel* table) const
  {
    const lrl_tdp::Arg di = lrl_tdp::Arg::default_instance();
    if (table->order() == 0) {
      // no table available
      return make_pair(di, di);
    } else if (table->order() == 1) {
      // in_slew does not affect out_slew
      // only consider load_cap
      const TableAxis* axis1 = table->axis1();
      size_t size1 = axis1->size();
      size_t idx_l = axis1->findAxisIndex(load_cap);
      size_t idx_r = idx_l + 1;
      if (idx_r >= size1) {
        idx_r = idx_l;
      }
      double y0 = table->value(idx_l, 0, 0);
      double y1 = table->value(idx_r, 0, 0);
      double xl = axis1->axisValue(idx_l);
      double xr = axis1->axisValue(idx_r);
      double gradient = (y1 - y0) / (xr - xl);
      lrl_tdp::Arg arg = pb_arg(gradient, xl, xr);
      return make_pair(arg, di);
    } else if (table->order() == 2) {
      // normal situation
      const TableAxis* axis1 = table->axis1();
      const TableAxis* axis2 = table->axis2();
      size_t size1 = axis1->size();
      size_t size2 = axis2->size();
      size_t axis_index1 = axis1->findAxisIndex(in_slew);
      size_t axis_index2 = axis2->findAxisIndex(load_cap);
      size_t idx1_l = axis_index1, idx1_r = axis_index1 + 1;
      if (size1 == 1) {
        idx1_r = idx1_l;
      }
      size_t idx2_l = axis_index2, idx2_r = axis_index2 + 1;
      if (size2 == 1) {
        idx2_r = idx2_l;
      }

      double y00 = table->value(idx1_l, idx2_l, 0);
      double y10 = table->value(idx1_r, idx2_l, 0);
      double y01 = table->value(idx1_l, idx2_r, 0);
      double y11 = table->value(idx1_r, idx2_r, 0);
      double x1l = axis1->axisValue(idx1_l);
      double x1u = axis1->axisValue(idx1_r);
      double x2l = axis2->axisValue(idx2_l);
      double x2u = axis2->axisValue(idx2_r);
      if (size1 == 1 && size2 == 1) {
        return make_pair(di, di);
      } else if (size1 == 1) {
        double gradient1 = (y01 - y00) / (x2u - x2l);
        lrl_tdp::Arg arg = pb_arg(gradient1, x2l, x2u);
        return make_pair(arg, di);
      } else if (size2 == 1) {
        double gradient2 = (y10 - y00) / (x1u - x1l);
        lrl_tdp::Arg arg = pb_arg(gradient2, x1l, x1u);
        return make_pair(di, arg);
      } else {
        double yx0 = y00 * (x1u - in_slew) / (x1u - x1l)
                     + y10 * (in_slew - x1l) / (x1u - x1l);
        double yx1 = y01 * (x1u - in_slew) / (x1u - x1l)
                     + y11 * (in_slew - x1l) / (x1u - x1l);
        double gradient1 = (yx1 - yx0) / (x2u - x2l);
        lrl_tdp::Arg arg1 = pb_arg(gradient1, x2l, x2u);

        double y0x = y00 * (x2u - load_cap) / (x2u - x2l)
                     + y01 * (load_cap - x2l) / (x2u - x2l);
        double y1x = y10 * (x2u - load_cap) / (x2u - x2l)
                     + y11 * (load_cap - x2l) / (x2u - x2l);
        double gradient2 = (y1x - y0x) / (x1u - x1l);
        lrl_tdp::Arg arg2 = pb_arg(gradient2, x1l, x1u);
        return make_pair(arg1, arg2);
      }
    }
    // unreachable
    return make_pair(di, di);
  }

  void fillArc(Edge* e,TimingArc* arc)
  {
    Slack from = ad_.maxFromSlack(e);
    Slack to = ad_.maxToSlack(e);
    
    lrl_tdp::Arc* pb_arc = tdp_->add_arcs();
    pb_arc->set_instance(ad_.instName(ad_.to(e)));
    pb_arc->set_pin_start(ad_.fromName(arc));
    pb_arc->set_pin_end(ad_.toName(arc));
    *(pb_arc->mutable_input_slack()) = pb_delay(from);
    *(pb_arc->mutable_output_slack()) = pb_delay(to);
    *(pb_arc->mutable_input_arrival()) = pb_delay(ad_.maxFromArrival(e));
    *(pb_arc->mutable_output_arrival()) = pb_delay(ad_.maxToArrival(e));
    *(pb_arc->mutable_arc_delay()) = pb_delay(ad_.arcDelay(e, arc));
    *(pb_arc->mutable_capacitance())
        = pb_cap(ad_.loadCap(e, arc), ad_.capLimit(arc->to()));

    double in_slew
        = ad_.slew(e->from(ad_.graph()), arc->fromEdge()->asRiseFall());
    double load_cap = ad_.loadCap(e, arc);

    GateTableModel* model = dynamic_cast<GateTableModel*>(arc->model());
    const TableModel* delay_model = model->delayModel();
    const TableModel* slew_model = model->slewModel();
    auto [d_loadcap_arg, d_slew_arg]
        = gradients(in_slew, load_cap, delay_model);
    auto [s_loadcap_arg, s_slew_arg] = gradients(in_slew, load_cap, slew_model);
    *(pb_arc->mutable_delay_loadcap_arg()) = d_loadcap_arg;
    *(pb_arc->mutable_delay_slope_arg()) = d_slew_arg;
    *(pb_arc->mutable_transition_loadcap_arg()) = s_loadcap_arg;
    *(pb_arc->mutable_transition_slope_arg()) = s_slew_arg;
    *(pb_arc->mutable_input_slew()) = pb_delay(
        ad_.slew(e->from(ad_.graph()), arc->fromEdge()->asRiseFall()));
    *(pb_arc->mutable_output_slew())
        = pb_delay(ad_.slew(e->to(ad_.graph()), arc->toEdge()->asRiseFall()));
  }

  void fillEdge(Vertex* v)
  {
    VertexInEdgeIterator iter(v, ad_.graph());

    RiseFall* rf = nullptr;
    VertexInEdgeIterator it(v, ad_.graph());
    while (it.hasNext()) {
      Edge* e = it.next();
      TimingArcSet* arc_set = e->timingArcSet();
      if (e->isWire() || arc_set->role()->isTimingCheck()) {
        continue;
      }
      Arrival input_arr = ad_.maxFromArrival(e);
      Arrival output_arr = ad_.maxToArrival(e);
      for (TimingArc* arc : arc_set->arcs()) {
        if (arc->role() == TimingRole::regSetClr()) {
          continue;
        }
        Delay delay = ad_.arcDelay(e, arc);
        if (fuzzyEqual(input_arr + delay, output_arr)) {
          rf = ad_.toRf(arc);
        }
      }
    }

    while (iter.hasNext()) {
      Edge* e = iter.next();
      TimingArcSet* arc_set = e->timingArcSet();
      if (e->isWire() || arc_set->role()->isTimingCheck()) {continue;}

      for (TimingArc* arc : arc_set->arcs()) {
        if (arc->role() == TimingRole::regSetClr()) {
          continue;
        }
        if (arc && e) {
          if (rf) {
            if (ad_.toRf(arc)->name() == rf->name()) {
              fillArc(e, arc);
            }
          } else {
            fillArc(e, arc);
          }
        }
      }
    }
  }

  lrl_tdp::Arg worstArcSetupArg(Vertex* v) const
  {
    TimingArcInfo worst = TimingArcInfo();
    VertexInEdgeIterator iter(v, ad_.graph());
    while (iter.hasNext()) {
      Edge* e = iter.next();
      TimingArcSet* arc_set = e->timingArcSet();
      if (e->isWire() || !arc_set->role()->isTimingCheck()) {
        continue;
      }

      for (TimingArc* arc : arc_set->arcs()) {
        Delay arc_delay = ad_.arcDelay(e, arc);
        TimingArcInfo rhs = {arc_delay, arc, e};
        if (!rhs.valid()) {
          continue;
        }
        TimingRole* role = e->role();
        if (!role->isTimingCheck() || role->pathMinMax() != MinMax::max()) {
          continue;
        }
        if (worst.arc_delay < arc_delay) {
          worst = rhs;
        }
      }
    }
    if (worst.valid()) {
      TimingArc* arc = worst.arc;
      Edge* e = worst.edge;
      double in_slew
          = ad_.slew(e->from(ad_.graph()), arc->fromEdge()->asRiseFall());
      double load_cap = ad_.loadCap(e, arc);

      CheckTableModel* model = dynamic_cast<CheckTableModel*>(arc->model());
      const TableModel* setup_model = model->model();
      auto [_, slope_arg] = gradients(in_slew, load_cap, setup_model);
      return slope_arg;
    }
    return lrl_tdp::Arg::default_instance();
  }

  bool isEndPoint(Vertex* v)
  {
    VertexOutEdgeIterator next_iter(v, ad_.graph());
    bool has_next = next_iter.hasNext();
    VertexInEdgeIterator pre_iter(v, ad_.graph());
    bool has_pre = pre_iter.hasNext();
    return (!has_next && has_pre);
  }

  void fillSeqPin(Vertex* v)
  {
    bool is_clk = ad_.isClock(v);
    bool is_clk_gate
        = ad_.network()->libertyPort(v->pin())->libertyCell()->isClockGate();

    if (!is_clk && !isEndPoint(v)) {
      return;
    }
    Slack s = ad_.maxSlack(v);

    PortDirection* dir = ad_.dir(v);
    lrl_tdp::SeqPin* pb_seq_pin = tdp_->add_seq_pins();
    *(pb_seq_pin->mutable_slack()) = pb_delay(s);
    pb_seq_pin->set_instance(ad_.instName(v));
    pb_seq_pin->set_pin(ad_.portName(v));

    pb_seq_pin->clear_cap();
    pb_seq_pin->clear_setup_slope_arg();
    bool is_start;
    if (is_clk && !is_clk_gate) {
      is_start = dir->isAnyInput();
    } else {
      is_start = dir->isAnyOutput();
    }
    if (is_start) {
      *(pb_seq_pin->mutable_cap()) = pb_cap(ad_.loadCap(v), ad_.capLimit(v));
    } else {
      *(pb_seq_pin->mutable_setup_slope_arg()) = worstArcSetupArg(v);
    }
    *(pb_seq_pin->mutable_arrival()) = pb_delay(ad_.maxArrival(v));
    *(pb_seq_pin->mutable_slew()) = pb_delay(
        std::max(ad_.slew(v, RiseFall::rise()), ad_.slew(v, RiseFall::fall())));
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

    bool is_clk_gate
        = ad_.network()->libertyPort(v->pin())->libertyCell()->isClockGate();
    if (ad_.isDrvr(v) && !is_clk_gate)
      return fillEdge(v);
  }

  void fillPort(Vertex* v)
  {
    PortDirection* dir = ad_.dir(v);
    lrl_tdp::Port* pb_port = tdp_->add_ports();

    pb_port->set_port_name(ad_.name(v));
    bool is_input = dir->isAnyInput();
    if (is_input) {
      *(pb_port->mutable_capacitance())
          = pb_cap(ad_.loadCap(v), ad_.capLimit(v));
    } else {
      *(pb_port->mutable_slew()) = pb_delay(std::max(
          ad_.slew(v, RiseFall::rise()), ad_.slew(v, RiseFall::fall())));
    }
    *(pb_port->mutable_output_slack()) = pb_delay(ad_.maxSlack(v));
    *(pb_port->mutable_arrival()) = pb_delay(ad_.maxArrival(v));
  }

  lrl_tdp::Delay pb_delay(double seconds)
  {
    lrl_tdp::Delay ret = lrl_tdp::Delay::default_instance();
    ret.set_seconds(seconds);
    return ret;
  }

  lrl_tdp::Capacitance pb_cap(double load_cap, double max_cap)
  {
    lrl_tdp::Capacitance ret = lrl_tdp::Capacitance::default_instance();
    ret.set_load_cap(load_cap);
    ret.set_max_cap(max_cap);
    return ret;
  }

  lrl_tdp::Arg pb_arg(double gradient, double lb, double ub) const
  {
    lrl_tdp::Arg ret = lrl_tdp::Arg::default_instance();
    ret.set_gradient(gradient);
    ret.set_lower_boundary(lb);
    ret.set_upper_boundary(ub);
    return ret;
  }

public:
  IncrTdpDataFiller() = delete;
  IncrTdpDataFiller(lrl_tdp::IncrTdpData* t, Sta* s)
    : tdp_(t), ad_(s) {};
  IncrTdpDataFiller(lrl_tdp::IncrTdpData* t, const StaAdapter& s)
      : tdp_(t), ad_(s) {};
  virtual ~IncrTdpDataFiller() = default;

  void fillIncrTdpData(Resizer* rsz)
  {
    tdp_->set_mode("default");
    tdp_->set_corner(ad_.corner()->name());
    VertexIterator iter(ad_.graph());
    while (iter.hasNext()) {
      Vertex* v = iter.next();
      if (ad_.isPort(v)) {
        fillPort(v);
      } else {
        fillCell(v);
      }
    }
    // fill other variables
    tdp_->set_wire_res(ad_.wireRes(rsz));
    tdp_->set_wire_cap(ad_.wireCap(rsz));
  }

  void test_gradient()
  {
    VertexIterator iter(ad_.graph());
    int cnt = 0;
    int legal_cnt = 0;
    int delay_cnt = 0;
    int slew_cnt = 0;
    while (iter.hasNext()) {
      Vertex* v = iter.next();
      if (ad_.isPort(v)) {
        continue;
      }
      LibertyPort* port = ad_.port(v);
      if (!port) {
        continue;
      }
      LibertyCell* cell = port->libertyCell();
      if (!cell) {
        continue;
      }
      if (cell->hasSequentials() || cell->isClockGate()) {
        if (ad_.isDrvr(v)) {
          VertexOutEdgeIterator iter(v, ad_.graph());
          if (!iter.hasNext()) {
            continue;
          }
        }
      }
      bool is_clk_gate
          = ad_.network()->libertyPort(v->pin())->libertyCell()->isClockGate();
      if (!(ad_.isDrvr(v) && !is_clk_gate)) {
        continue;
      }
      VertexInEdgeIterator edges(v, ad_.graph());
      while (edges.hasNext()) {
        Edge* e = edges.next();
        TimingArcSet* arc_set = e->timingArcSet();
        if (e->isWire() || arc_set->role()->isTimingCheck()) {
          continue;
        }

        for (TimingArc* arc : arc_set->arcs()) {
          ArcDelay in_slew
              = ad_.slew(e->from(ad_.graph()), arc->fromEdge()->asRiseFall());
          double load_cap = ad_.loadCap(e, arc);
          Delay arc_delay = ad_.arcDelay(e, arc);

          GateTableModel* m = dynamic_cast<GateTableModel*>(arc->model());
          const TableModel* delay_model = m->delayModel();
          const TableModel* slew_model = m->slewModel();
          auto [d_loadcap, d_slope] = gradients(in_slew, load_cap, delay_model);
          auto [s_loadcap, s_slope] = gradients(in_slew, load_cap, slew_model);
          double a1 = d_loadcap.gradient(), a2 = d_slope.gradient();
          double b1 = s_loadcap.gradient(), b2 = s_slope.gradient();
          double delta_rate1 = rand() % 11;
          if (rand() % 2) {
            delta_rate1 *= -1;
          }
          delta_rate1 = 1 + delta_rate1 / 100.0;
          double delta_rate2 = rand() % 11;
          if (rand() % 2) {
            delta_rate2 *= -1;
          }
          delta_rate2 = 1 + delta_rate2 / 100.0;
          double delta_inslew = in_slew * delta_rate1;
          double delta_cload = load_cap * delta_rate2;
          auto new_delay = arc_delay + a1 * delta_cload + a2 * delta_inslew;
          GateTableModel* model = dynamic_cast<GateTableModel*>(arc->model());
          const TableModel* delay_table = model->delayModel();
          double trial_delay = delay_table->findValue(
              in_slew + delta_inslew, load_cap + delta_cload, 0);
          if (fabs(trial_delay - new_delay) <= fabs(trial_delay * 0.05)) {
            legal_cnt++;
            delay_cnt++;
          }

          const TableModel* slew_table = model->slewModel();
          double out_slew = slew_table->findValue(in_slew, load_cap, 0);
          auto new_slope = out_slew + b1 * delta_cload + b2 * delta_inslew;
          double trial_slope = slew_table->findValue(
              in_slew + delta_inslew, load_cap + delta_cload, 0);
          if (fabs(trial_slope - new_slope) <= fabs(trial_slope * 0.05)) {
            legal_cnt++;
            slew_cnt++;
          };
          cnt += 2;
        }
      }
    }
    printf("test %d times, pass %d\n", cnt, legal_cnt);
    printf("delay pass: %d, pass rate: %.2lf%%\n",
           delay_cnt,
           100.0 * delay_cnt / (cnt / 2.0));
    printf("slope pass: %d, pass rate: %.2lf%%\n",
           slew_cnt,
           100.0 * slew_cnt / (cnt / 2.0));
  }
};

class TdpDataFiller
{
  lrl_tdp::TdpData* tdp_;
  StaAdapter ad_;

 public:
  TdpDataFiller() = delete;
  TdpDataFiller(lrl_tdp::TdpData* t, Sta* s) : tdp_(t), ad_(s){};
  virtual ~TdpDataFiller() = default;

  void fillTdpData(Resizer* rsz)
  {
    lrl_tdp::IncrTdpData* incr_tdp = tdp_->add_data();
    IncrTdpDataFiller incr_tdp_filler(incr_tdp, ad_);
    incr_tdp_filler.fillIncrTdpData(rsz);
  }
};

bool TdpDataWriter::serialize(std::string const& file_name)
{
  if (LINDEN_IN_REGRESSION()) {
    spdlog::set_pattern("%v");  // no timestamp
  }
  if (file_name.empty()) {
    spdlog::error("[LDN-TDP] no file_name input");
    return false;
  }
  Sta* sta = ord_->getSta();
  sta->checkCapacitanceLimitPreamble();
  sta->ensureClkNetwork();

  lrl_tdp::TdpData tdp;
  TdpDataFiller filler(&tdp, sta);
  filler.fillTdpData(ord_->getResizer());

  std::fstream output(file_name.c_str(),
                      std::ios::out | std::ios::trunc | std::ios::binary);
  if (!tdp.SerializeToOstream(&output)) {
    spdlog::error("[LDN-TDP] fail to write out pb file");
    return false;
  }
  spdlog::info("[LDN-TDP] write tdp data pb file to: {}",file_name);
  return true;
}

////////////////////////////////////////////////////////
//
//  class IncrTdpDataWriter
//
////////////////////////////////////////////////////////

bool IncrTdpDataWriter::serialize(std::string const& file_name)
{
  if(file_name.empty()) {
    spdlog::error("[LDN-TDP] no file_name input");
    return false;
  }

  if (LINDEN_IN_REGRESSION()) {
    spdlog::set_pattern("%v"); // no timestamp
  }

  Sta* sta = ord_->getSta();
  sta->checkCapacitanceLimitPreamble();
  sta->ensureClkNetwork();

  lrl_tdp::IncrTdpData tdp;
  IncrTdpDataFiller filler(&tdp, sta);
  filler.fillIncrTdpData(ord_->getResizer());

  std::fstream output(file_name.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
  if(!tdp.SerializeToOstream(&output)) {
    spdlog::error("[LDN-TDP] fail to write out pb file");
    return false;
  }
  spdlog::info("[LDN-TDP] write incr tdp data pb file to: {}",file_name);
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
  std::cout << "<mode>: " << tdp.mode() << std::endl;
  std::cout << "<corner>: " << tdp.corner() << std::endl;
  std::cout << "<wire_cap>: " << tdp.wire_cap() << std::endl;
  std::cout << "<wire_res>: " << tdp.wire_res() << std::endl;
  // ports
  for (int i = 0; i < tdp.ports_size(); ++i) {
    auto& pb_port = tdp.ports(i);
    std::cout << "port_name: " << pb_port.port_name() << std::endl;
    if (pb_port.has_capacitance()) {
      std::cout << "c_pins: " << pb_port.capacitance().load_cap()
                << std::endl;
      std::cout << "c_max: " << pb_port.capacitance().max_cap() << std::endl;
    }
    if (pb_port.has_output_slack()) {
      std::cout << "output_slack: " << pb_port.output_slack().seconds()
                << std::endl;
    }
    std::cout << "arrival: " << pb_port.arrival().seconds() << std::endl;
    std::cout << "slew: " << pb_port.slew().seconds() << std::endl;
    std::cout << "----------------------" << std::endl;
  }
  // seq_pins
  for (int i = 0; i < tdp.seq_pins_size(); ++i) {
    auto& pb_seq_pin = tdp.seq_pins(i);
    std::cout << "seq_instance: " << pb_seq_pin.instance() << std::endl;
    std::cout << "seq_pin_name: " << pb_seq_pin.pin() << std::endl;
    std::cout << "slack: " << pb_seq_pin.slack().seconds() << std::endl;
    if (pb_seq_pin.has_cap()) {
      std::cout << "  c_pins: " << pb_seq_pin.cap().load_cap() << std::endl;
    }
    if (pb_seq_pin.has_setup_slope_arg()) {
      std::cout << "  setup_args: " << pb_seq_pin.setup_slope_arg().gradient()
                << std::endl;
    }
    std::cout << "arrival: " << pb_seq_pin.arrival().seconds() << std::endl;
    std::cout << "slew: " << pb_seq_pin.slew().seconds() << std::endl;
    std::cout << "----------------------" << std::endl;
  }
  // arcs
  for (int i = 0; i < tdp.arcs_size(); ++i) {
    auto& pb_arc = tdp.arcs(i);
    std::cout << "  instance: " << pb_arc.instance() << std::endl;
    std::cout << "  pin_start: " << pb_arc.pin_start() << std::endl;
    std::cout << "  pin_end: " << pb_arc.pin_end() << std::endl;
    std::cout << "  input_slack: " << pb_arc.input_slack().seconds()
              << std::endl;
    std::cout << "  output_slack: " << pb_arc.output_slack().seconds()
              << std::endl;
    std::cout << "  input_arrival: " << pb_arc.input_arrival().seconds()
              << std::endl;
    std::cout << "  output_arrival: " << pb_arc.output_arrival().seconds()
              << std::endl;
    std::cout << "  input_slack: " << pb_arc.input_slew().seconds()
              << std::endl;
    std::cout << "  output_slack: " << pb_arc.output_slew().seconds()
              << std::endl;
    std::cout << "  arc_delay: " << pb_arc.arc_delay().seconds() << std::endl;
    std::cout << "  c_max: " << pb_arc.capacitance().max_cap() << std::endl;
    std::cout << "  c_pins: " << pb_arc.capacitance().load_cap() << std::endl;
    std::cout << "  delay_args: " << std::endl;
    std::cout << "    loadcap_args: " << pb_arc.delay_loadcap_arg().gradient()
              << std::endl;
    std::cout << "    slope_args: " << pb_arc.delay_slope_arg().gradient()
              << std::endl;
    std::cout << "  transition_args: " << std::endl;
    std::cout << "    loadcap_args: "
              << pb_arc.transition_loadcap_arg().gradient() << std::endl;
    std::cout << "    slope_args: " << pb_arc.transition_slope_arg().gradient()
              << std::endl;
    std::cout << "----------------------" << std::endl;
  }
}

bool IncrTdpDataWriter::deserialize(std::string const& file)
{
  if (LINDEN_IN_REGRESSION()) {
    spdlog::set_pattern("%v");  // no timestamp
  }
  lrl_tdp::IncrTdpData tdp;
  std::fstream input(file.c_str(), std::ios::in | std::ios::binary);
  if(!tdp.ParseFromIstream(&input)) {
    spdlog::error("[LDN-TDP] Failed to parse from: {}", file);
    return false;
  }
  spdlog::info("[LDN-TDP] read pb_file from: {}",file);
  report(tdp);
  return true;
}

void report(lrl_tdp::TdpData const& tdp)
{
  for (int i = 0; i < tdp.data_size(); ++i) {
    auto& pb_data = tdp.data(i);
    report(pb_data);
    std::cout << "----------------------" << std::endl;
  }
}

bool TdpDataWriter::deserialize(std::string const& file)
{ 
  if (LINDEN_IN_REGRESSION()) {
    spdlog::set_pattern("%v");  // no timestamp
  }
  lrl_tdp::TdpData tdp;
  std::fstream input(file.c_str(), std::ios::in | std::ios::binary);
  if (!tdp.ParseFromIstream(&input)) {
    spdlog::error("[LDN-TDP] Failed to parse from: {}", file);
    return false;
  }
  spdlog::info("[LDN-TDP] read pb_file from: {}",file);
  report(tdp.data(0));

  return true;
}

}  // namespace rsz
