#include "gpl/lindengp.h"

#include <iostream>

#include "graphics.h"
#include "initialPlace.h"
#include "initialPlaceBicgstab.h"
#include "initialPlaceDef.h"
#include "initialPlaceDirichlet.h"
#include "initialPlaceEV.h"
#include "linden/gpl/placer_config.hpp"
#include "linden/lefdef_dumper.hpp"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "placerBase.h"
#include "rsz/Resizer.hh"
#include "sta/Fuzzy.hh"
#include "sta/Units.hh"
#include "utl/Logger.h"
#include "fassert.hpp"

using namespace std;
using utl::GPL;

namespace gpl {

LindenGlobalPlace::LindenGlobalPlace()
    : db_(nullptr),
      log_(nullptr),
      pb_(nullptr),
      initialPlacePolicy_(""),
      linden_db_(nullptr),
      lindengp_(nullptr),
      density_(1.0),
      overflow_(0.1),
      use_uniform_density_(false),
      zigzag_(false),
      net_weight_max_(1.9),
      timing_overflows_{},
      padLeft_(0),
      padRight_(0),
      gui_debug_(false),
      gui_debug_pause_iterations_(10),
      gui_debug_update_iterations_(10),
      gui_debug_initial_(false)
{
}

void LindenGlobalPlace::reset() {
  lindengp_.reset();
  linden_db_.reset();
  pb_.reset();
}

void LindenGlobalPlace::setTimingDriven(bool timing_driven)
{
  if (timing_driven) {
    timing_overflows_ = {79, 64, 49, 29, 21, 15};
  } else {
    timing_overflows_ = {};
  }
}

void LindenGlobalPlace::initPlacerBase()
{
  if (!pb_) {
    PlacerBaseVars pbVars;
    pbVars.padLeft = padLeft_;
    pbVars.padRight = padRight_;
    pbVars.skipIoMode = false;
    pb_ = std::make_shared<PlacerBase>(db_, pbVars, log_);
  }
}

std::shared_ptr<linden::db::Database> LindenGlobalPlace::create_linden_db()
{
  FASSERT(pb_ != nullptr);
  auto const& die = pb_->die();
  linden::geom::Rect core_region(
      die.coreLx(), die.coreLy(), die.coreUx(), die.coreUy());

  linden::db::Database::Config cfg = {
    core_region : core_region,
    cells : pb_->insts().size(),  // number of cells;
    nets : pb_->nets().size(),    // max number of nets;
    iterm_pins : 0,               // max number of iterm_pins
    bterm_pins : 0,               // max number of bterm_pins;
  };
  for (auto& pin : pb_->pins()) {
    if (pin->isITerm()) {
      ++cfg.iterm_pins;
    } else {
      assert(pin->isBTerm());
      ++cfg.bterm_pins;
    }
  }

  auto linden_db = std::make_shared<linden::db::Database>(cfg);
  for (auto& inst : pb_->insts()) {
    linden::geom::Rect rect(
        inst->lx(), inst->ly(), inst->ux(), inst->uy());
    int64_t inst_area = inst->area();
    bool is_macro = inst->isMacro();
    bool is_fixed = inst->isFixed();
    if (is_fixed) {
      auto intersect = rect.intersect(core_region);
      if (intersect && intersect->area() > 0) {
        rect = *intersect;
      }
    }
    if (inst->dbInst() != nullptr) {
      void* odb_instance = inst->dbInst();
      linden_db->add_cell(linden::db::Cell(odb_instance,
                                           inst->dbInst()->getName(),
                                           rect,
                                           inst_area,
                                           is_macro,
                                           is_fixed));
    } else {
      linden_db->add_cell(linden::db::Cell(rect, is_fixed));
    }
  }
  for (auto& net : pb_->nets()) {
    linden_db->add_net(
        linden::db::Net((void*) net->dbNet(), net->dbNet()->getName()));
  }
  for (auto& pin : pb_->pins()) {
    auto o_net = linden_db->odb2net(pin->net()->dbNet());
    assert(o_net);
    if (pin->instance()) {
      assert(pin->isITerm());
      auto o_cell = linden_db->odb2cell(pin->instance()->dbInst());
      assert(o_cell);
      void* term = pin->dbITerm();
      std::string name = pin->dbITerm()->getMTerm()->getName();
      linden::geom::Point pin_offset(pin->offsetCx(), pin->offsetCy());
      linden_db->add_pin(
          linden::db::ITerm(term, *o_net, name, *o_cell, pin_offset));
    } else {
      void* term = nullptr;
      std::string name = "";
      if (pin->isITerm()) {
        term = pin->dbITerm();
        name = pin->dbITerm()->getMTerm()->getName();
      } else {
        term = pin->dbBTerm();
        name = pin->dbBTerm()->getName();
      }
      linden::geom::Point pin_center(pin->cx(), pin->cy());
      linden_db->add_pin(linden::db::BTerm(term, *o_net, name, pin_center));
    }
  }
  return linden_db;
}

void LindenGlobalPlace::run()
{
  initPlacerBase();
  doInitialPlace();
  if (!linden_db_) {
    linden_db_ = create_linden_db();
  }
  if (!lindengp_) {
    linden::gpl::PlacerConfig::PlacerType ty
        = linden::gpl::PlacerConfig::NPLACE;
    linden::gpl::PlacerConfig cfg(ty);
    cfg.target_density = density_;
    cfg.target_overflow = overflow_;
    cfg.use_uniform_density = use_uniform_density_;
    cfg.zigzag = zigzag_;
    {
      linden::dp::LefDefDumper dumper;
      dumper.run(*ord::OpenRoad::openRoad());
      cfg.input_def = dumper.def();
      cfg.tech_lef = dumper.tech_lef();
      cfg.lib_lefs = dumper.lib_lefs();
    }
    lindengp_ = linden::gpl::Placer::create(linden_db_, cfg, log_);
  }
  std::unique_ptr<Graphics> graphics;
  if (Graphics::guiActive()) {
    graphics = make_unique<Graphics>(log_, linden_db_, pb_);
  }
  const int MAX_ITER = 5000;
  if (!timing_overflows_.empty()) {
    rsz::Resizer* rs_ = ::ord::OpenRoad::openRoad()->getResizer();
    rs_->resizeSlackPreamble();
  }
  for (size_t iter = 0; iter < MAX_ITER; iter++) {
    auto ret = lindengp_->one_step_move(iter);
    if (graphics) {
      bool update
          = (iter == 0 || (iter + 1) % gui_debug_update_iterations_ == 0);
      bool pause = (iter == 0 || (iter + 1) % gui_debug_pause_iterations_ == 0);
      if (update) {
        graphics->cellPlot(pause);
      }
    }
    if (!ret) {
      log_->metric("gp::iter", iter+1);
      break;
    }
    auto overflow = *ret;
    if (!zigzag_ && overflow <= overflow_) {
      log_->metric("gp::iter", iter+1);
      break;
    }

    if (!timing_overflows_.empty()
        && overflow * 100 < timing_overflows_.front()) {
      if (!updateNetWeights()) {
        // escape timing-driven if problem occured
        timing_overflows_.clear();
      } else {
        timing_overflows_.pop_front();
      }
    }
  }

  if (graphics) {
    graphics->status("End placement");
    graphics->cellPlot(true);
  }

  update_db();
  report_metrics();
  reset();
}

void LindenGlobalPlace::update_db()
{  // update db
  for (auto& cell : linden_db_->cells()) {
    if (cell.movable() && cell.is_instance()) {
      odb::dbInst* inst = static_cast<odb::dbInst*>(cell.odb_instance());
      inst->setPlacementStatus(odb::dbPlacementStatus::PLACED);
      const linden::geom::Rect& r = cell.rect();
      // pad awareness on X coordinates
      inst->setLocation(r.lx() + pb_->siteSizeX() * pb_->padLeft(), r.ly());
    }
  }
}

bool LindenGlobalPlace::updateNetWeights()
{
  update_db();
  rsz::Resizer* rs = ::ord::OpenRoad::openRoad()->getResizer();
  rs->findResizeSlacks();

  // get Top 10% worst resize nets
  sta::NetSeq& worst_slack_nets = rs->resizeWorstSlackNets();

  if (worst_slack_nets.empty()) {
    log_->warn(GPL, 1114, "No net slacks found. Timing-driven mode disabled.");
    return false;
  }

  // min/max slack for worst 10% nets
  sta::Slack slack_min = rs->resizeNetSlack(worst_slack_nets[0]);
  sta::Slack slack_max
      = rs->resizeNetSlack(worst_slack_nets[worst_slack_nets.size() - 1]);

  log_->info(GPL, 1100, "worst slack {:.3g}", slack_min);

  if (sta::fuzzyInf(slack_min)) {
    log_->warn(GPL, 1102, "No slacks found. Timing-driven mode disabled.");
    return false;
  }

  size_t weighted_net_count = 0;
  for (auto& net : pb_->nets()) {
    auto dbNet = net->dbNet();
    // default weight
    double net_weight = 1.0;
    if (net->pins().size() > 1) {
      float net_slack = rs->resizeNetSlack(dbNet);
      if (net_slack < slack_max) {
        // weight(min_slack) = net_weight_max_
        // weight(max_slack) = 1
        net_weight = 1
                     + (net_weight_max_ - 1) * (slack_max - net_slack)
                           / (slack_max - slack_min);
        weighted_net_count++;
      }
      debugPrint(log_,
                 GPL,
                 "linden-tdp",
                 3,
                 "timing: net:{} slack:{} weight:{}",
                 dbNet->getName(),
                 net_slack,
                 net_weight);
    }
    linden_db_->set_net_timing_weight(dbNet, net_weight);
  }
  log_->info(GPL, 1103, "Weighted {} nets.", weighted_net_count);
  return true;
}

void LindenGlobalPlace::report_metrics()
{
  rsz::Resizer* rs = ::ord::OpenRoad::openRoad()->getResizer();
  rs->estimateParasitics(rsz::ParasiticsSrc::placement);
  auto sta = ::ord::OpenRoad::openRoad()->getSta();

  const int DIGITS = 2;
  auto total_negative_slack = sta->totalNegativeSlack(sta::MinMax::max());

  sta::Slack worst_slack;
  sta::Vertex* worst_vertex;
  sta->worstSlack(sta::MinMax::max(), worst_slack, worst_vertex);

  auto tns = sta->units()->timeUnit()->asString(total_negative_slack, DIGITS);
  auto wns = sta->units()->timeUnit()->asString(worst_slack, DIGITS);

  log_->metric("gp::tns", tns);
  log_->metric("gp::wns", wns);
}

void LindenGlobalPlace::setInitialPlacePolicy(const char* policy)
{
  initialPlacePolicy_ = policy != nullptr ? std::string(policy) : "";
}

void LindenGlobalPlace::doInitialPlace()
{
  FASSERT(pb_ != nullptr);
  std::unique_ptr<InitialPlace> ip_(nullptr);

  InitialPlaceVars ipVars;
  ipVars.maxIter = 20;
  ipVars.minDiffLength = 1500;
  ipVars.maxSolverIter = 100;
  ipVars.maxFanout = 200;
  ipVars.netWeightScale = 800;
  ipVars.debug = gui_debug_initial_;

  if (initialPlacePolicy_ == "def") {
    ip_ = std::move(
        std::unique_ptr<InitialPlace>(new InitialPlaceDef(ipVars, pb_, log_)));
  } else if (initialPlacePolicy_ == "ev") {
    ip_ = std::move(
        std::unique_ptr<InitialPlace>(new InitialPlaceEV(ipVars, pb_, log_)));
  } else if (initialPlacePolicy_ == "dirichlet") {
    ip_ = std::move(std::unique_ptr<InitialPlace>(
        new InitialPlaceDirichlet(ipVars, pb_, log_)));
  } else {  // default
    ip_ = std::move(std::unique_ptr<InitialPlace>(
        new InitialPlaceBicgstab(ipVars, pb_, log_)));
  }
  ip_->run();
}

}  // namespace gpl
