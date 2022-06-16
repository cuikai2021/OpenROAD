#include "lrl/Laurel.hh"

#include "gui/gui.h"
#include "ord/OpenRoad.hh"
#include "sta/StaMain.hh"

namespace sta {
extern const char* Laurel_tcl_inits[];
}

namespace lrl {

using namespace operations_research;
using namespace sta;
using namespace rsz;

extern "C" {
extern int Laurel_Init(Tcl_Interp* interp);
}

void Laurel::initLaurel(OpenRoad* openroad,
                        Tcl_Interp* interp,
                        Logger* logger,
                        Gui* gui,
                        dbDatabase* db,
                        dbSta* sta,
                        SteinerTreeBuilder* stt_builder,
                        GlobalRouter* global_router)
{
  openroad_ = openroad;
  logger_ = logger;
  gui_ = gui;
  db_ = db;
  block_ = nullptr;
  sta_ = sta;
  stt_builder_ = stt_builder;
  global_router_ = global_router;
  incr_groute_ = nullptr;
  db_network_ = sta->getDbNetwork();
  copyState(sta);
  // Define swig TCL commands.
  Laurel_Init(interp);
  // Eval encoded sta TCL sources.
  evalTclInit(interp, sta::Laurel_tcl_inits);
}

////////////////////////////////////////////////////////
//
//  For fixing hold
//
////////////////////////////////////////////////////////

sta::Map<Name, Vertex*> Laurel::makeNameVertexMap()
{
  sta::Map<Name, Vertex*> name_vertex_map;
  VertexIterator vertex_iter(sta_->graph());
  while (vertex_iter.hasNext()) {
    Vertex* vertex = vertex_iter.next();
    Name name = vertex->name(sta_->sdcNetwork());
    name_vertex_map.insert(name, vertex);
  }
  return name_vertex_map;
}

LibertyCell* Laurel::findHoldBuffer(Delay min_limit, int& count)
{
  LibertyCell* max_buffer = nullptr;
  Delay max_delay = INF;
  const Delay uncertainty = options_->uncertainty() / LRL_H_STA_SCALING;
  count = LRL_S_INT_INF;
  for (LibertyCell* buffer : buffer_cells_) {
    Delay buffer_min_delay = bufferHoldDelay(buffer);
    int min_count = std::ceil(min_limit / buffer_min_delay);
    Delay fixed_delay = buffer_min_delay * (Delay) (min_count);
    // if ((max_buffer == nullptr || buffer_min_delay < max_delay)
    //     && fuzzyLess(min_limit, buffer_min_delay)) {
    if (fuzzyLess(min_limit, fixed_delay)
        && fuzzyLess(fixed_delay, min_limit + uncertainty)
        && min_count < count) {
      max_buffer = buffer;
      max_delay = buffer_min_delay;
      count = min_count;
    }
  }
  if (max_buffer)
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               4,
               "choose buffer {}, delay {}",
               max_buffer->name(),
               delayAsString(max_delay, this, 3));
  return max_buffer;
}

// function copied and updated from OpenRoad/Resizer
void Laurel::insertHoldDelay(Vertex* drvr,
                             int buffer_count,
                             PinSeq& load_pins,
                             bool loads_have_out_port,
                             LibertyCell* buffer_cell)
{
  Pin* drvr_pin = drvr->pin();
  Instance* parent = sta_->getDbNetwork()->topInstance();
  Net* drvr_net
      = sta_->network()->isTopLevelPort(drvr_pin)
            ? sta_->getDbNetwork()->net(sta_->getDbNetwork()->term(drvr_pin))
            : sta_->getDbNetwork()->net(drvr_pin);
  Net *in_net, *out_net;
  if (loads_have_out_port) {
    // Verilog uses nets as ports, so the net connected to an output port has
    // to be preserved.
    // Move the driver pin over to gensym'd net.
    in_net = makeUniqueNet();
    Port* drvr_port = sta_->network()->port(drvr_pin);
    Instance* drvr_inst = sta_->network()->instance(drvr_pin);
    sta_->disconnectPin(drvr_pin);
    sta_->connectPin(drvr_inst, drvr_port, in_net);
    out_net = drvr_net;
  } else {
    in_net = drvr_net;
    out_net = makeUniqueNet();
  }

  parasiticsInvalid(in_net);
  // Spread buffers between driver and load center.
  Point drvr_loc = sta_->getDbNetwork()->location(drvr_pin);
  Point load_center = findCenter(load_pins);
  int dx = (load_center.x() - drvr_loc.x()) / (buffer_count + 1);
  int dy = (load_center.y() - drvr_loc.y()) / (buffer_count + 1);

  Net* buf_in_net = in_net;
  Instance* buffer = nullptr;
  LibertyPort *input, *output;
  buffer_cell->bufferPorts(input, output);
  // drvr_pin->in_net->hold_buffer1->net2->hold_buffer2->out_net...->load_pins
  for (int i = 0; i < buffer_count; i++) {
    Net* buf_out_net = (i == buffer_count - 1) ? out_net : makeUniqueNet();
    // drvr_pin->drvr_net->hold_buffer->net2->load_pins
    string buffer_name = makeUniqueInstName("hold");
    buffer = sta_->getDbNetwork()->makeInstance(
        buffer_cell, buffer_name.c_str(), parent);
    journalMakeBuffer(buffer);
    inserted_buffer_count_++;
    designAreaIncr(area(sta_->getDbNetwork()->cell(buffer_cell)));

    sta_->connectPin(buffer, input, buf_in_net);
    sta_->connectPin(buffer, output, buf_out_net);
    Point buffer_loc(drvr_loc.x() + dx * i, drvr_loc.y() + dy * i);
    setLocation(buffer, buffer_loc);
    buf_in_net = buf_out_net;
    parasiticsInvalid(buf_out_net);
  }

  for (Pin* load_pin : load_pins) {
    Net* load_net = sta_->network()->isTopLevelPort(load_pin)
                        ? sta_->network()->net(sta_->network()->term(load_pin))
                        : sta_->network()->net(load_pin);
    if (load_net != out_net) {
      Instance* load = sta_->getDbNetwork()->instance(load_pin);
      Port* load_port = sta_->getDbNetwork()->port(load_pin);
      sta_->disconnectPin(load_pin);
      sta_->connectPin(load, load_port, out_net);
    }
  }
  // Pin* buffer_out_pin = sta_->network()->findPin(buffer, output);
  // resizeToTargetSlew(buffer_out_pin, false);
}

Pin* Laurel::findFanoutPin(HoldFixer* fixer, Vertex* pred, Vertex* succ) const
{
  if (!succ->isDriver(sta_->network()))
    return succ->pin();
  VertexOutEdgeIterator pred_edge_iter(pred, sta_->graph());
  while (pred_edge_iter.hasNext()) {
    Edge* pred_edge = pred_edge_iter.next();
    Vertex* fanout = pred_edge->to(sta_->graph());
    VertexInEdgeIterator succ_edge_iter(succ, sta_->graph());
    while (succ_edge_iter.hasNext()) {
      Edge* succ_edge = succ_edge_iter.next();
      Vertex* fanin = succ_edge->from(sta_->graph());
      if (fanin == fanout)
        return fanin->pin();
    }
  }
  return nullptr;
}

bool Laurel::insertBuffer(HoldFixer* fixer,
                          Name vertex_name,
                          sta::Map<Name, Vertex*> name_vertex_map)
{
  WirePairSet wire_pair_set = fixer->getAllWirePairFrom(vertex_name);
  PinSeq load_pins;
  Vertex* insert_from = name_vertex_map.at(vertex_name);
  bool loads_have_out_port = false;
  Delay max_padding = LRL_H_ZERO;
  Delay min_padding = INF;
  for (auto wire_pair : wire_pair_set) {
    Delay padding = fixer->getWirePadding(wire_pair);
    if (fuzzyLess(LRL_H_ROUNDING_LIMIT, padding)) {
      Vertex* fanout = name_vertex_map.at(wire_pair.second);
      Pin* fanout_pin = findFanoutPin(fixer, insert_from, fanout);
      if (fanout_pin != nullptr) {
        load_pins.push_back(fanout_pin);
        if (sta_->network()->direction(fanout_pin)->isAnyOutput()
            && sta_->network()->isTopLevelPort(fanout_pin))
          loads_have_out_port = true;

        if (fuzzyLess(max_padding, padding))
          max_padding = padding;
        if (fuzzyLess(padding, min_padding) || fuzzyInf(min_padding))
          min_padding = padding;
      }
    }
  }

  const Delay uncertainty = options_->uncertainty() / LRL_H_STA_SCALING;
  Delay padding = max_padding;
  if (!fuzzyLess(max_padding - min_padding, uncertainty)) {
    logger_->warn(
        utl::LRL,
        1000,
        "padding diff in all {} fanout from {} higher then uncertainty",
        load_pins.size(),
        insert_from->name(sta_->sdcNetwork()));
    // return false;
    padding = min_padding;
  }

  int buffer_count = 1;
  LibertyCell* buffer_cell = findHoldBuffer(max_padding, buffer_count);
  while (!buffer_cell) {
    // buffer_count *= LRL_H_BUFFER_SPILIT;
    // padding /= LRL_H_BUFFER_SPILIT;
    // buffer_cell = findHoldBuffer(padding);
    return false;
  }

  if (buffer_cell != nullptr && !load_pins.empty()) {
    insertHoldDelay(
        insert_from, buffer_count, load_pins, loads_have_out_port, buffer_cell);
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               4,
               "inserted {} buffer ",
               std::to_string(buffer_count));
  }
  return true;
}

bool Laurel::insertAllBuffer(HoldFixer* fixer)
{
  bool ret = true;
  findBuffers();
  sta::Map<Name, Vertex*> name_vertex_map = makeNameVertexMap();
  VertexNameSet vio_pred_set = fixer->getNonZeroPaddingPred();
  for (auto vertex_name : vio_pred_set) {
    if (!insertBuffer(fixer, vertex_name, name_vertex_map))
      ret = false;
  }
  logger_->info(
      utl::LRL, 1110, "Totally inserted {} buffers", inserted_buffer_count_);
  return ret;
}

bool Laurel::fixHoldCore()
{
  HoldFixer* fixer = new HoldFixer(openroad_, options_);
  HoldResult result = fixer->resolve();
  bool ret = true;
  if (result != MPSolver::ResultStatus::OPTIMAL) {
    logger_->warn(
        utl::LRL, 1001, "Linear Programming failed, return {}", result);
    ret = false;
  } else {
    ret = insertAllBuffer(fixer);
  }
  delete fixer;
  return ret;
}

void Laurel::fixHold()
{
  init();
  inserted_buffer_count_ = 0;
  incrementalParasiticsBegin();
  bool ret = fixHoldCore();
  // Leave the parasitices up to date.
  updateParasitics();
  incrementalParasiticsEnd();
  if (!ret) {
    logger_->warn(utl::LRL, 1002, "fail to fix all hold violations");
  }
}

////////////////////////////////////////////////////////
//
//  For fixing Setup
//
////////////////////////////////////////////////////////

LibertyCell* Laurel::findResizeCell(Vertex* vertex, Delay target_delay)
{
  Pin* drvr_pin = vertex->pin();
  LibertyPort* drvr_port = sta_->network()->libertyPort(drvr_pin);
  LibertyCell* cell = drvr_port->libertyCell();
  LibertyCellSeq* equiv_cells = sta_->equivCells(cell);
  PathRef path = sta_->vertexWorstSlackPath(vertex, max_);
  const DcalcAnalysisPt* dcalc_ap = path.dcalcAnalysisPt(sta_);
  const char* drvr_port_name = drvr_port->name();
  const Delay load_cap = graph_delay_calc_->loadCap(drvr_pin, dcalc_ap);
  int lib_ap = dcalc_ap->libertyIndex();
  Delay extreme_delay = INF;
  LibertyCell* resize_cell = nullptr;
  for (LibertyCell* equiv : *equiv_cells) {
    LibertyCell* equiv_corner = equiv->cornerCell(lib_ap);
    LibertyPort* equiv_drvr = equiv_corner->findLibertyPort(drvr_port_name);
    Delay equiv_delay = gateDelay(equiv_drvr, load_cap, dcalc_ap);
    if (fuzzyLess(equiv_delay, extreme_delay)
        && fuzzyLess(target_delay, equiv_delay)) {
      extreme_delay = equiv_delay;
      resize_cell = equiv;
    }
  }
  return resize_cell;
}

bool Laurel::resizeCell(Vertex* vertex, Delay target_delay)
{
  LibertyCell* resize_cell = findResizeCell(vertex, target_delay);
  if (resize_cell) {
    Instance* drvr = network_->instance(vertex->pin());
    if (replaceCell(drvr, resize_cell, true)) {
      logger_->info(utl::LRL,
                    1304,
                    "resize cell {} -> {}",
                    vertex->name(sta_->sdcNetwork()),
                    resize_cell->name());
      resize_count_++;
      return true;
    }
  }
  logger_->warn(utl::LRL,
                1201,
                "cannot resize cell ({} -> {})",
                vertex->name(sta_->sdcNetwork()),
                delayAsString(target_delay, sta_, 3));
  return false;
}

bool Laurel::resizeAllCells(SetupFixer* fixer)
{
  bool ret = true;
  VertexSet changed_vertex = fixer->getResizeVertex();
  for (auto vertex : changed_vertex) {
    Delay resize = fixer->getResizeValue(vertex);
    Delay new_delay = fixer->getNewDelay(vertex);
    logger_->info(utl::LRL,
                  1301,
                  " {} : {} (new_delay = {})",
                  vertex->name(sta_->sdcNetwork()),
                  delayAsString(resize, sta_, 3),
                  delayAsString(new_delay, sta_, 3));
    if (!resizeCell(vertex, new_delay))
      ret = false;
  }
  logger_->info(utl::LRL,
                1305,
                "Totally resized {} cells",
                std::to_string(resize_count_));
  return ret;
}

void Laurel::fixSetup()
{
  SetupFixer fixer = SetupFixer(openroad_, options_);
  SetupResult status = fixer.resolve();
  bool ret = false;
  if (status == MinCostFlow::OPTIMAL) {
    fixer.report();
    if (fixer.remap()) {
      ret = resizeAllCells(&fixer);
    }
  }
  if (!ret)
    logger_->warn(utl::LRL, 1202, "Fail to resize all cells");
  else
    logger_->info(utl::LRL, 1306, "Succeed to resize all cells");
}

////////////////////////////////////////////////////////
//
//  class LaurelAdapter
//
////////////////////////////////////////////////////////

VertexSet LaurelAdapter::getAllRelatedEnds(VertexSet ends, bool ignore_clk)
{
  VertexSet fanin_roots = findFaninRoots(ends);
  VertexSet related_ends = ends;
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
  return related_ends;
}

Vertex* LaurelAdapter::findRealVertex(Name vertex_name) const
{
  VertexIterator vertex_iter(sta_->graph());
  while (vertex_iter.hasNext()) {
    Vertex* test_vertex = vertex_iter.next();
    if (!vertex_name.compare(test_vertex->name(sta_->sdcNetwork())))
      return test_vertex;
  }
  return nullptr;
}

}  // namespace lrl
