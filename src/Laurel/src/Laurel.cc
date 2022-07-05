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
  if (wire_bud_)
    delete wire_bud_;
}

////////////////////////////////////////////////////////
//
//  For fixing hold
//
////////////////////////////////////////////////////////

bool Laurel::fixHoldCore()
{
  HoldFixer fixer(openroad_, options_.get());
  HoldResult result = fixer.resolve();
  bool ret = true;
  if (result != MPSolver::ResultStatus::OPTIMAL) {
    logger_->warn(
        utl::LRL, 1001, "Linear Programming failed, return {}", result);
    ret = false;
  } else {
    ret = fixer.insertAllBuffer();
  }
  inserted_buffer_count_ = fixer.getInsertedBufferCnt();
  return ret;
}

int Laurel::fixHold()
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
  return inserted_buffer_count_;
}

Slack Laurel::getHoldSlack(Vertex* drvr)
{
  return sta_->vertexSlack(drvr, MinMax::min());
}

Slack Laurel::getHoldSlack(Pin* drvr_pin)
{
  if (!wire_bud_)
    return INF;
  Vertex* vertex = wire_bud_->getVertex(drvr_pin);
  if (!vertex)
    return INF;
  return getHoldSlack(vertex);
}

////////////////////////////////////////////////////////
//
//  For fixing Setup
//
////////////////////////////////////////////////////////

bool Laurel::resizeAllCells(SetupFixer* fixer)
{
  bool ret = true;
  sta::Map<Vertex*, LibertyCell*> queue;
  fixer->getResizeQueue(queue);
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
    resize_count_++;
  }
  logger_->info(utl::LRL,
                1305,
                "Totally resized {} cells",
                std::to_string(resize_count_));
  return ret;
}

int Laurel::fixSetup()
{
  resize_count_ = 0;
  SetupFixer fixer = SetupFixer(openroad_, options_.get());
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
  return resize_count_;
}

////////////////////////////////////////////////////////
//
//  For wire budgeting
//
////////////////////////////////////////////////////////

void Laurel::showAllWireBud(WireBudgeting* guider)  // interface debug function
{
  const VertexPairSet& pairs = guider->getAllWirePairs();
  int cnt = 0;
  int slack_cnt = 0;
  logger_->info(utl::LRL, 1310, "test wire budgeting interface");
  for (auto& wire : pairs) {
    Vertex* pred = wire.first;
    Vertex* succ = wire.second;

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
    wire_bud_ = new WireBudgetingRC(openroad_, options_.get());
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
  wire_bud_ = new WireBudgeting(openroad_, options_.get());
  wireBudgetingCore();
}

void Laurel::wireBudgeting(float factor)
{
  if (wire_bud_)
    delete wire_bud_;
  wire_bud_ = new WireBudgetingFake(openroad_, options_.get(), factor);
  wireBudgetingCore();
}

bool Laurel::checkVertexBudgeted(Vertex* pred, Vertex* succ)
{
  if (pred && succ) {
    VertexPair pin_wire = VertexPair(pred, succ);
    if (!wire_bud_->hasWire(pin_wire)) {
      logger_->warn(utl::LRL,
                    1204,
                    "not found budget len result for vertex {} -> {}",
                    pred->name(sta_->sdcNetwork()),
                    succ->name(sta_->sdcNetwork()));
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

void LaurelAdapter::getAllRelatedEnds(VertexSet ends,
                                      bool ignore_clk,
                                      VertexSet& related_ends)
{
  VertexSet fanin_roots = findFaninRoots(ends);
  related_ends = ends;
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
