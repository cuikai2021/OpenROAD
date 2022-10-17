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

void Laurel::deleteLaurel()
{
  if (wire_bud_)
    delete wire_bud_;
  if (ccopt_fixer_)
    delete ccopt_fixer_;
}

////////////////////////////////////////////////////////
//
//  For fixing hold
//
////////////////////////////////////////////////////////

bool Laurel::fixHoldCore(bool multi_mode, VertexSet* v)
{
  HoldFixer fixer(openroad_, options_.get());
  fixer.initAlgorithm(multi_mode, v);
  HoldResult result = fixer.resolve();
  bool ret = true;
  if (result != MPSolver::ResultStatus::OPTIMAL) {
    debugPrint(logger_, utl::LRL, "LRL", 1, "Linear Programming failed, return {}", result);
    infeasible_++;
    ret = false;
  } else {
    fixer.hold_buffers_ = hold_buffers_.get();
    ret = fixer.insertAllBuffer();
  }
  inserted_buffer_count_ += fixer.getInsertedBufferCnt();
  return ret;
}

int Laurel::fixHold(bool multi_mode, bool partial)
{
  init();
  inserted_buffer_count_ = 0;
  infeasible_ = 0;
  parasiticPreamble(openroad_->getResizer());
  incrementalParasiticsBegin();
  int const min_task_size = 50;
  bool ret = true;
  if (!partial)
    ret = fixHoldCore(multi_mode);
  else {
    VertexSet task;
    HoldFixerTaskManager manager(openroad_, options_.get());
    manager.findTasks(multi_mode);
    int const len = manager.taskSize();
    debugPrint(logger_, utl::LRL, "LRL", 1, "totally {} tasks", len);
    for (size_t i = 0; i<len; ++i) {
      VertexSet* s = manager.taskSet(i);
      task.insert(s->begin(), s->end());
      if (task.size() >= min_task_size || i == len - 1) { 
        debugPrint(logger_, utl::LRL, "LRL", 1, "solving group: {}", i);
        if(!fixHoldCore(multi_mode, &task))
          ret = false;
        task.clear();
      }
    }
  }
  updateParasitics();
  incrementalParasiticsEnd();
  if (!ret) {
    logger_->warn(utl::LRL, 1002, "fail to fix all hold violations");
  }
  logger_->info(
      utl::LRL, 1110, 
      "Totally inserted {} buffers , {} infeasible",
      inserted_buffer_count_,
      infeasible_);
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
  }
  updateParasitics();
  logger_->info(utl::LRL,
                1305,
                "Totally resized {} cells",
                queue.size());
  if (queue.size() == 0)
    return false;
  resize_count_ += queue.size();
  return ret;
}

int Laurel::fixSetup(int iter)
{
  resize_count_ = 0;
  SetupResult status = MinCostFlow::OPTIMAL;
  bool has_violation = true;
  bool iterative = iter > 1;
  parasiticPreamble(openroad_->getResizer());
  incrementalParasiticsBegin();
  for (int i = 1; i <= iter && status == MinCostFlow::OPTIMAL; ++i) {
    SetupFixer* fixer = new SetupFixer(openroad_, options_.get());
    has_violation = fixer->initAlgorithm(iterative);
    bool fixed = false;
    if (has_violation) {
      logger_->info(utl::LRL, 1306, "**fix setup, iter = {} **", i);
      status = fixer->resolve();
      if (status == MinCostFlow::OPTIMAL) {
        fixer->remap();
        fixer->report();
        fixed = resizeAllCells(fixer);
      }
    }
    delete fixer;
    updateParasitics();
    if(!fixed)
      break;
  }
  incrementalParasiticsEnd();
  return resize_count_;
}

int Laurel::ccopt()
{
  resize_count_ = 0;
  if (ccopt_fixer_)
    delete ccopt_fixer_;
  parasiticPreamble(openroad_->getResizer());
  incrementalParasiticsBegin();
  ccopt_fixer_ = new ClockConcurrentFixer(openroad_, options_.get());
  ccopt_fixer_->initAlgorithm();
  SetupResult status = ccopt_fixer_->resolve();
  if (status == MinCostFlow::OPTIMAL) {
    ccopt_fixer_->report();
    ccopt_fixer_->remap();
    if (options_->getDebugReport())
      ccopt_fixer_->report_skews();
    resizeAllCells(ccopt_fixer_);
  }
  updateParasitics();
  incrementalParasiticsEnd();
  return resize_count_;
}

float Laurel::usefulSkew(Vertex const* vertex) const
{
  if (!ccopt_fixer_)
    return LRL_H_ZERO;
  return ccopt_fixer_->getUsefulSkew(vertex);
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
  if (!pred || !succ)
    return LRL_S_ZERO;
  return getWireBudRate(pred, succ);
}

////////////////////////////////////////////////////////
//
//  utils
//
////////////////////////////////////////////////////////

bool Laurel::isWorstPathFromInPort(Vertex* vertex, MinMax const* min_max)
{
  PathRef worst_path = sta_->vertexWorstSlackPath(vertex, min_max);
  PathExpanded expanded(&worst_path, sta_);
  if (expanded.size() > 1) {
    Vertex* start = pathVertex(expanded, expanded.startIndex());
    if (sta_->network()->isTopLevelPort(start->pin()))
      return true;
  }
  return false;
}

void Laurel::reportViolations(MinMax const* min_max, bool ignore_top_ports)
{
  struct Failure
  {
    Vertex* vertex;
    Slack slack;
    bool operator<(Failure const& other) const
    {
      return this->slack < other.slack;
    }
  };

  initFastOpt(openroad_);
  sta_->findRequireds();

  const int report_limit = 10;
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  VertexSet* ends = sta_->search()->endpoints();
  std::vector<Failure> failures;
  for (Vertex* vertex : *ends) {
    Slack slack = sta_->vertexSlack(vertex, min_max);
    if (!fuzzyInf(slack) && fuzzyLess(slack, slack_margin))
      if (!ignore_top_ports
          || (!isWorstPathFromInPort(vertex, min_max)
              && !sta_->network()->isTopLevelPort(vertex->pin())))
        failures.push_back({vertex, slack});
  }
  logger_->info(utl::LRL,
                1502,
                "Laurel report {} violations in {}\n show worst ends: (max {})",
                failures.size(),
                min_max == max_ ? "setup" : "hold",
                report_limit);
  sort(failures.begin(), failures.end());
  for (int i = 0; i < failures.size() && i < report_limit; ++i)
    printf("  %s : %s\n",
           delayAsString(failures[i].slack, this, 3),
           failures[i].vertex->name(sdc_network_));
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

}  // namespace lrl
