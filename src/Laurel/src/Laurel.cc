#include "lrl/Laurel.hh"
#include "lrl/LPSolver.hh"
#include "lrl/MCFSolver.hh"
#include "lrl/SetupFixer.hh"
#include "lrl/WireBudgeting.hh"
#include "lrl/HoldFixer.hh"
#include "lrl/Buffering.hh"
#include "lrl/Replacer.hh"

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

bool Laurel::fixHoldCore(bool multi_mode, HoldBuffering* b, VertexSet* v)
{
  LPSolver solver(LRL_H_SOLVER);
  HoldFixer fixer(openroad_, options_.get(), &solver);
  fixer.initAlgorithm(multi_mode, v);
  auto result = solver.solve();
  bool ret = true;
  if (result != MPSolver::ResultStatus::OPTIMAL) {
    debugPrint(logger_, utl::LRL, "LRL", 1, "Linear Programming failed, return {}", result);
    infeasible_++;
    ret = false;
  } else {
    ret = b->insertAllBuffer(&fixer);
  }
  inserted_buffer_count_ += b->getInsertedBufferCnt();
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
  HoldBuffering buffering(openroad_, options_.get());
  if (!partial)
    ret = fixHoldCore(multi_mode, &buffering);
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
        if(!fixHoldCore(multi_mode, &buffering, &task))
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

int Laurel::fixSetup(int iter)
{
  resize_count_ = 0;
  SetupReplacer replacer(openroad_, options_.get());
  parasiticPreamble(openroad_->getResizer());
  incrementalParasiticsBegin();
  for (int i = 1; i <= iter; ++i) {
    SetupFixer fixer(openroad_, options_.get());
    bool fixed = false;
    if (fixer.initAlgorithm(iter > 1)) {
      logger_->info(utl::LRL, 1306, "**fix setup, iter = {} **", i);
      if (fixer.resolve() == MinCostFlow::OPTIMAL) {
        fixer.remap();
        fixer.report();
        fixed = replacer.resizeAllCells(&fixer);
        resize_count_ += replacer.getResizeCnt();
      }
    }
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
  SetupReplacer replacer(openroad_, options_.get());
  if (ccopt_fixer_)
    delete ccopt_fixer_;
  parasiticPreamble(openroad_->getResizer());
  incrementalParasiticsBegin();
  ccopt_fixer_ = new ClockConcurrentFixer(openroad_, options_.get());
  ccopt_fixer_->initAlgorithm();
  if (ccopt_fixer_->resolve() == MinCostFlow::OPTIMAL) {
    ccopt_fixer_->report();
    ccopt_fixer_->remap();
    if (options_->getDebugReport())
      ccopt_fixer_->report_skews();
    replacer.resizeAllCells(ccopt_fixer_);
    resize_count_ = replacer.getResizeCnt();
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
  const WireSet& pairs = guider->getAllWirePairs();
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
  MCFResult status = wire_bud_->resolve();
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
    Wire pin_wire = Wire(pred, succ);
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
    return MCFSolver::zero;
  }
  uint64 max_length = MCFSolver::zero;
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
    return MCFSolver::zero;
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
    return MCFSolver::zero;
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
    int const i = expanded.startIndex();
    Vertex* start = expanded.path(i)->vertex(sta_);
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

}  // namespace lrl
