#include "lrl/Replacer.hh"

namespace lrl {

////////////////////////////////////////////////////////
//
//  class SetupReplacer
//
////////////////////////////////////////////////////////

LibertyCell* SetupReplacer::findResizeCell(Vertex* v, ResizeInfo const& i)
{
  Delay extreme_delay = LRL_H_ZERO;
  LibertyCell* resize_cell = nullptr;

  Pin* out_pin = v->pin();
  LibertyPort* out_port = sta_->network()->libertyPort(out_pin);
  LibertyCell* cell = out_port->libertyCell();
  LibertyCellSeq* equiv_cells = sta_->equivCells(cell);

  if (equiv_cells) {
    auto& factor = i.info;
    DcalcAnalysisPt const* dcalc_ap = factor->dcalc_ap;
    int const lib_ap = dcalc_ap->libertyIndex();
    LibertyPort* in_port = factor->in_port;
    Capacitance const load_cap = graph_delay_calc_->loadCap(out_pin, dcalc_ap);
    Resistance const& prev_drive = factor->prev_drive;
    const char* out_port_name = out_port->name();
    const char* in_port_name = in_port->name();
    for (LibertyCell* equiv : *equiv_cells) {
      LibertyCell* equiv_corner = equiv->cornerCell(lib_ap);
      LibertyPort* equiv_drvr = equiv_corner->findLibertyPort(out_port_name);
      LibertyPort* equiv_input = equiv_corner->findLibertyPort(in_port_name);
      Delay equiv_delay = gateDelay(equiv_drvr, load_cap, dcalc_ap)
                          + prev_drive * equiv_input->capacitance();
      if (fuzzyLessEqual(equiv_delay, i.new_delay + LRL_H_ROUNDING_LIMIT)
          && fuzzyLess(extreme_delay, equiv_delay)) {
        extreme_delay = equiv_delay;
        resize_cell = equiv;
      }
    }
  }
  return resize_cell;
}

void SetupReplacer::getResizeQueue(sta::Map<Vertex*, LibertyCell*>& queue,
                                   SetupReplacerSource* source)
{
  ResizeMap changed_vertex;
  source->getResizeVertex(changed_vertex);
  for (auto& ele : changed_vertex.map) {
    Vertex* succ = ele.first;
    ResizeInfo const& info = ele.second;
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               4,
               " {} (new_delay = {})",
               succ->name(sta_->sdcNetwork()),
               delayAsString(info.new_delay, sta_, 3));
    LibertyCell* resize_cell = findResizeCell(succ, info);
    if (resize_cell)
      queue.insert(succ, resize_cell);
    else
      logger_->warn(utl::LRL,
                    1201,
                    "not found resize cell ({} -> {})",
                    succ->name(sta_->sdcNetwork()),
                    delayAsString(info.new_delay, sta_, 3));
  }
}

bool SetupReplacer::resizeAllCells(SetupReplacerSource* source)
{
  bool ret = true;
  sta::Map<Vertex*, LibertyCell*> queue;
  getResizeQueue(queue, source);
  for (auto& q : queue) {
    Vertex* vertex = q.first;
    Instance* drvr = sta_->network()->instance(vertex->pin());
    LibertyCell* resize_cell = q.second;
    if (!replaceCell(drvr, resize_cell, true)) {
      ret = false;
      logger_->warn(utl::LRL,
                    1206,
                    "cannot resize cell ({})",
                    vertex->name(sta_->sdcNetwork()));
      continue;
    }
  }
  updateParasitics();
  logger_->info(utl::LRL, 1305, "Totally resized {} cells", queue.size());
  resize_count_ = queue.size();
  if (queue.size() == 0)
    return false;
  return ret;
}

}  // namespace lrl