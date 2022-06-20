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

Laurel::~Laurel()
{
  delete options_;
  if (wire_bud_)
    delete wire_bud_;
}

////////////////////////////////////////////////////////
//
//  For fixing hold
//
////////////////////////////////////////////////////////

LibertyCell* Laurel::findHoldBuffer(Delay min_limit)
{
  LibertyCell* max_buffer = nullptr;
  Delay max_delay = INF;
  const Delay uncertainty = options_->uncertainty() / LRL_H_STA_SCALING;
  for (LibertyCell* buffer : buffer_cells_) {
    Delay buffer_min_delay = bufferHoldDelay(buffer);
    // if ((max_buffer == nullptr || buffer_min_delay < max_delay)
    //     && fuzzyLess(min_limit, buffer_min_delay)) {
    if (fuzzyLess(min_limit, buffer_min_delay)
        && fuzzyLess(buffer_min_delay, min_limit + uncertainty)) {
      max_buffer = buffer;
      max_delay = buffer_min_delay;
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

bool Laurel::insertBuffer(HoldFixer* fixer, Name vertex_name)
{
  WirePairSet wire_pair_set = fixer->getAllWirePairFrom(vertex_name);
  PinSeq load_pins;
  Vertex* insert_from = findRealVertex(vertex_name);
  bool loads_have_out_port = false;
  Delay max_padding = LRL_H_ZERO;
  Delay min_padding = INF;
  for (auto wire_pair : wire_pair_set) {
    Delay padding = fixer->getWirePadding(wire_pair);
    if (fuzzyLess(LRL_H_ROUNDING_LIMIT, padding)) {
      Pin* fanout_pin = findFanoutPin(wire_pair);
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
  LibertyCell* buffer_cell = findHoldBuffer(max_padding);
  while (!buffer_cell) {
    buffer_count *= LRL_H_BUFFER_SPILIT;
    padding /= LRL_H_BUFFER_SPILIT;
    buffer_cell = findHoldBuffer(padding);
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
  vio_pred_set_ = fixer->getNonZeroPaddingPred();
  for (auto vertex_name : vio_pred_set_) {
    if (!insertBuffer(fixer, vertex_name))
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

bool Laurel::fixHoldNoInsertion()
{
  bool ret = false;
  init();
  HoldFixer* fixer = new HoldFixer(openroad_, options_);
  HoldResult result = fixer->resolve();
  if (result == MPSolver::ResultStatus::OPTIMAL) {
    ret = true;
    vio_pred_set_ = fixer->getNonZeroPaddingPred();
  }
  delete fixer;
  return ret;
}

bool Laurel::hasHoldVio(Vertex* drvr)
{
  Name vertex_name = drvr->name(sta_->sdcNetwork());
  return vio_pred_set_.hasKey(vertex_name);
}

bool Laurel::hasHoldVio(Pin* drvr_pin)
{
  Name vertex_name = sta_->network()->name(drvr_pin);
  return vio_pred_set_.hasKey(vertex_name);
}

////////////////////////////////////////////////////////
//
//  For fixing Setup
//
////////////////////////////////////////////////////////

bool Laurel::resizeAllCells(SetupFixer* fixer)
{
  bool ret = true;
  sta::Map<Vertex*, LibertyCell*> queue = fixer->getResizeQueue();
  for (auto q : queue) {
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
    resize_count_++;
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
  fixer.initAlgorithm();
  SetupResult status = fixer.resolve();
  bool ret = false;
  if (status == MinCostFlow::OPTIMAL) {
    fixer.report();
    ret = fixer.remap();
    ret = resizeAllCells(&fixer);
  }
  if (!ret)
    logger_->warn(utl::LRL, 1202, "Fail to resize all cells");
  else
    logger_->info(utl::LRL, 1306, "Succeed to resize all cells");
}

////////////////////////////////////////////////////////
//
//  For wire budgeting
//
////////////////////////////////////////////////////////

void Laurel::showAllWireBud(WireBudgeting* guider)  // interface debug function
{
  WirePairSet pairs = guider->getAllWirePairs();
  int cnt = 0;
  int slack_cnt = 0;
  logger_->info(utl::LRL, 1310, "test wire budgeting interface");
  for (auto wire : pairs) {
    Vertex* pred = guider->getVertex(wire.first);
    Vertex* succ = guider->getVertex(wire.second);

    uint64 max_length = getWireBudMaxLen(pred, succ);
    Delay bud_rate = getWireBudRate(pred, succ);
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               1,
               "{} -> {}, max_length = {}, bud_rate = {}",
               pred->name(sta_->sdcNetwork()),
               succ->name(sta_->sdcNetwork()),
               max_length,
               bud_rate);
    if (bud_rate > 1.0)
      slack_cnt++;
    cnt++;
  }
  printf("iterated %d connections\n", cnt);
  printf("slacked %d connections\n", slack_cnt);
}

void Laurel::wireBudgetingCore()
{
  bool ret = false;
  wire_bud_->initAlgorithm();
  SetupResult status = wire_bud_->resolve();
  if (status == MinCostFlow::OPTIMAL) {
    wire_bud_->report();
    if (wire_bud_->remap())
      ret = true;
  }
  if (!ret)
    logger_->warn(utl::LRL, 1203, "Fail to do wire budgeting");
  else
    logger_->info(utl::LRL, 1308, "Succeed to do wire budgeting");
  if (options_->getDebugReport())
    showAllWireBud(wire_bud_);
}

void Laurel::wireBudgeting()
{
  if (wire_bud_)
    delete wire_bud_;

  if (options_->hasSpefFile())
    return wireBudgetingSpef(options_->spefFileName());

  if (options_->isWireBudModeRC()) {
    wire_bud_ = new WireBudgetingRC(openroad_, options_);
    return wireBudgetingCore();
  }

  return wireBudgeting(0.2);
}

void Laurel::wireBudgetingRC()
{
  options_->setWireBudModeRC();
  wireBudgeting();
  options_->resetWireBudMode();
}

void Laurel::wireBudgetingSpef(const char* filename)
{
  if (wire_bud_)
    delete wire_bud_;
  options_->setSpefFile(filename);
  wire_bud_ = new WireBudgeting(openroad_, options_);
  wireBudgetingCore();
}

void Laurel::wireBudgeting(float factor)
{
  if (wire_bud_)
    delete wire_bud_;
  wire_bud_ = new WireBudgetingFake(openroad_, options_, factor);
  wireBudgetingCore();
}

bool Laurel::checkVertexBudgeted(Vertex* pred, Vertex* succ)
{
  if (pred && succ) {
    Name fanin_name = pred->name(sta_->sdcNetwork());
    Name fanout_name = succ->name(sta_->sdcNetwork());
    WirePair pin_wire = WirePair(fanin_name, fanout_name);
    if (!wire_bud_->hasWire(pin_wire)) {
      logger_->warn(utl::LRL,
                    1204,
                    "not found budget len result for vertex {} -> {}",
                    fanin_name,
                    fanout_name);
      return false;
    }
    return true;
  }
  logger_->warn(utl::LRL, 1205, "not found vertex (nullptr) in wire bud");
  return false;
}

uint64 Laurel::getWireBudMaxLen(Vertex* fanin, Vertex* fanout)
{
  if (!checkVertexBudgeted(fanin, fanout)) {
    return LRL_S_ZERO;
  }
  uint64 max_length = LRL_S_ZERO;
  Delay bud_rate = LRL_H_ZERO;  // unused
  wire_bud_->getNewWireLength(fanin, fanout, max_length, bud_rate);
  return max_length;
}

uint64 Laurel::getWireBudMaxLen(Pin* fanin, Pin* fanout)
{
  Vertex* pred = wire_bud_->getVertex(fanin);
  Vertex* succ = wire_bud_->getVertex(fanout);
  return getWireBudMaxLen(pred, succ);
}

Delay Laurel::getWireBudRate(Vertex* fanin, Vertex* fanout)
{
  if (!checkVertexBudgeted(fanin, fanout)) {
    return LRL_S_ZERO;
  }
  uint64 max_length = LRL_H_ZERO;  // unused
  Delay bud_rate = LRL_H_ZERO;
  wire_bud_->getNewWireLength(fanin, fanout, max_length, bud_rate);
  return bud_rate;
}

Delay Laurel::getWireBudRate(Pin* fanin, Pin* fanout)
{
  Vertex* pred = wire_bud_->getVertex(fanin);
  Vertex* succ = wire_bud_->getVertex(fanout);
  return getWireBudRate(pred, succ);
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

Pin* LaurelAdapter::findFanoutPin(WirePair wire_pair) const
{
  Vertex* pred = findRealVertex(wire_pair.first);
  Vertex* succ = findRealVertex(wire_pair.second);
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

Delay LaurelAdapter::getArrivalMax(Vertex* vertex) const
{
  Delay max_arrival = LRL_H_ZERO;
  VertexPathIterator path_iter(vertex, sta_);
  while (path_iter.hasNext()) {
    Path* path = path_iter.next();
    Delay arrival = path->arrival(sta_);
    if ((fuzzyLess(max_arrival, arrival)) && !fuzzyInf(arrival))
      max_arrival = arrival;
  }
  return max_arrival;
}

}  // namespace lrl
