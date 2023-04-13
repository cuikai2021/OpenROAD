#include "gpl/lindengp.h"
#include "linden/scanchain.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <vector>
#include <unordered_set>
#include <functional>

#include "sta/Liberty.hh"
#include "fassert.hpp"
#include "graphics.h"
#include "initialPlace.h"
#include "initialPlaceBicgstab.h"
#include "initialPlaceDef.h"
#include "initialPlaceDirichlet.h"
#include "initialPlaceEV.h"
#include "linden/gpl/placer_config.hpp"
#include "linden/lefdef_dumper.hpp"
#include "linden/rdp/congestion_estimation.hpp"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "db_sta/dbSta.hh"
#include "db_sta/dbNetwork.hh"
#include "placerBase.h"
#include "rsz/Resizer.hh"
#include "sta/Fuzzy.hh"
#include "sta/Units.hh"
#include "utl/Logger.h"
#include "envflag/envflag.hpp"
#include "cgr/congestionRepair.hpp"

using namespace std;
using utl::GPL;

IMPORT_F64_FLAG_WITH_DEFAULT(LINDEN_SCAN_ENABLE_WEIGHT);
IMPORT_F64_FLAG_WITH_DEFAULT(LINDEN_CLOCK_WEIGHT);
IMPORT_F64_FLAG_WITH_DEFAULT(LINDEN_RESET_WEIGHT);

namespace {
// only for internal use
struct IsScanChainsIgnoredPin
{
  vector<const odb::dbITerm*> scan_in_iterms;

  IsScanChainsIgnoredPin(const odb::dbSet<odb::dbScanChain>& scan_chains)
  {
    size_t scan_in_iterms_size = 0;
    for (auto scan_chain : scan_chains) {
      scan_in_iterms_size += scan_chain->getFloating().size();
      scan_in_iterms_size += scan_chain->getOrdered().size();
      scan_in_iterms_size += 1;
    }
    scan_in_iterms.reserve(scan_in_iterms_size);

    for (auto scan_chain : scan_chains) {
      for (auto floating_comp : scan_chain->getFloating()) {
        auto in_iterm = floating_comp->getScanIn();
        scan_in_iterms.push_back(in_iterm);
      }
      for (auto ordered_comps : scan_chain->getOrdered()) {
        auto in_iterm = ordered_comps->getComps().begin()->getScanIn();
        scan_in_iterms.push_back(in_iterm);
      }
      auto stop_iterm = scan_chain->getStop();
      scan_in_iterms.push_back(stop_iterm);
    }

    sort(scan_in_iterms.begin(), scan_in_iterms.end());
    // i.e. there are no duplicate values in vec.
    assert(std::adjacent_find(scan_in_iterms.begin(), scan_in_iterms.end())
           == scan_in_iterms.end());
  }

  bool operator()(const gpl::Pin* value)
  {
    return value->isITerm()
           && binary_search(
               scan_in_iterms.begin(), scan_in_iterms.end(), value->dbITerm());
  }
};
}  // unnamed namespace

namespace gpl {

LindenGlobalPlace::LindenGlobalPlace()
    : db_(nullptr),
      log_(nullptr),
      pb_(nullptr),
      linden_db_(nullptr),
      lindengp_(nullptr),
      banyan_cmap_result_(nullptr),
      initialPlacePolicy_(),
      density_(1.0),
      overflow_(0.1),
      use_uniform_density_(false),
      zigzag_(false),
      cgr_mode_(false),
      net_weight_max_(1.9),
      timing_overflows_{},
      routability_driven_(false),
      padLeft_(0),
      padRight_(0),
      gui_debug_(false),
      gui_debug_pause_iterations_(10),
      gui_debug_update_iterations_(10),
      gui_debug_initial_(false)
{
}

void LindenGlobalPlace::reset()
{
  pb_.reset();
  linden_db_.reset();
  lindengp_.reset();
  banyan_cmap_result_.reset();
}

void LindenGlobalPlace::setTimingDriven(bool timing_driven)
{
  if (timing_driven) {
    timing_overflows_ = {79, 64, 49, 29, 21, 15};
  } else {
    timing_overflows_ = {};
  }
}

void LindenGlobalPlace::setRoutabilityDriven(bool flag)
{
  routability_driven_ = flag;
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

namespace {

unordered_set<odb::dbNet*> nets_to_scanchain_enables()
{
  deque<linden::PairedCell> cells_on_sc = linden::cells_on_scanchains();
  unordered_set<odb::dbNet*> res;
  for(auto const& cell: cells_on_sc) {
    if (cell.scan_enable()) {
      const linden::PairedPin& se = *cell.scan_enable();
      odb::dbNet* net = se.db().getNet();
      if (net != nullptr) {
        res.insert(net);
      }
    }
  }
  return res;
}

unordered_set<odb::dbNet*> nets_to_clock(const deque<linden::PairedCell>& all_cells)
{
  unordered_set<odb::dbNet*> res;
  for(const linden::PairedCell& cell: all_cells) {
    for(const linden::PairedPin& pin: cell.clocks()) {
      odb::dbNet* net = pin.db().getNet();
      if (net != nullptr) {
        res.insert(net);
      }
    }
  }
  return res;
}

unordered_set<odb::dbNet*> nets_to_resets(const deque<linden::PairedCell>& all_cells)
{
  unordered_set<odb::dbNet*> res;
  for(const linden::PairedCell& cell: all_cells) {
    for(const linden::PairedPin& pin: cell.resets()) {
      odb::dbNet* net = pin.db().getNet();
      if (net != nullptr) {
        res.insert(net);
      }
    }
  }
  return res;
}

} // namespace

std::shared_ptr<linden::db::Database> LindenGlobalPlace::create_linden_db()
{
  FASSERT(pb_ != nullptr);
  auto const& die = pb_->die();
  linden::geom::Rect die_region(
      die.dieLx(), die.dieLy(), die.dieUx(), die.dieUy());
  linden::geom::Rect core_region(
      die.coreLx(), die.coreLy(), die.coreUx(), die.coreUy());

  linden::db::Database::Config cfg = {
    die_region : die_region,
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
    linden::geom::Rect rect(inst->lx(), inst->ly(), inst->ux(), inst->uy());
    int64_t inst_area = inst->area();
    bool is_macro = inst->isMacro();
    bool is_fixed = inst->isFixed();
    float max_density = inst->max_density();
    if (is_fixed) {
      auto intersect = rect.intersect(core_region);
      if (intersect && intersect->area() > 0) {
        rect = *intersect;
      }
    }
    if (inst->dbInst() != nullptr) {
      void* odb_instance = inst->dbInst();
      std::vector<linden::geom::Rect> obs_overlap_layer;
      if (is_macro) {
        auto master = inst->dbInst()->getMaster();
        for (auto obs : master->getObstructions()) {
          auto layer = obs->getTechLayer();
          if (layer->getType() == odb::dbTechLayerType::OVERLAP) {
            linden::geom::Rect rect(
                obs->xMin(), obs->yMin(), obs->xMax(), obs->yMax());
            obs_overlap_layer.push_back(rect);
          }
        }
      }
      if (obs_overlap_layer.empty()) {
        linden_db->add_cell(linden::db::Cell(odb_instance,
                                             inst->dbInst()->getName(),
                                             rect,
                                             inst_area,
                                             is_macro,
                                             is_fixed,
                                             max_density));
      } else {
        FASSERT(is_macro);
        inst_area = 0;
        for (const auto& r : obs_overlap_layer) {
          inst_area += r.area();
        }
        linden_db->add_cell(linden::db::Cell(odb_instance,
                                             inst->dbInst()->getName(),
                                             rect,
                                             obs_overlap_layer,
                                             inst_area,
                                             is_fixed,
                                             max_density));
      }
    } else {
      linden_db->add_cell(linden::db::Cell(rect, is_fixed, max_density));
    }
  }

  const unordered_set<odb::dbNet*>& se_nets = nets_to_scanchain_enables();
  debugPrint(log_, GPL, "linden", 1, "#nets connecting scan enables: {}", se_nets.size());
  const deque<linden::PairedCell>& all_cells_w_lib = linden::all_cells_with_lib();
  const unordered_set<odb::dbNet*>& clock_nets = nets_to_clock(all_cells_w_lib);
  debugPrint(log_, GPL, "linden", 1, "#nets connecting clocks: {}", clock_nets.size());
  const unordered_set<odb::dbNet*>& reset_nets = nets_to_clock(all_cells_w_lib);
  debugPrint(log_, GPL, "linden", 1, "#nets connecting resets: {}", reset_nets.size());
  for (auto& net : pb_->nets()) {
    linden::db::Net lnet((void*) net->dbNet(), net->dbNet()->getName());
    if (se_nets.count(net->dbNet()) > 0) {
      lnet.mut_weight() = LINDEN_SCAN_ENABLE_WEIGHT();
    }
    if (clock_nets.count(net->dbNet()) > 0) {
      lnet.mut_weight() = LINDEN_CLOCK_WEIGHT();
    }
    if (reset_nets.count(net->dbNet()) > 0) {
      lnet.mut_weight() = LINDEN_RESET_WEIGHT();
    }
    linden_db->add_net(std::move(lnet));
  }
  IsScanChainsIgnoredPin isScanChainsIgnoredPin(
      pb_->db()->getChip()->getBlock()->getScanChains());
  for (auto& pin : pb_->pins()) {
    if (isScanChainsIgnoredPin(pin)) {
      continue;
    }
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

void LindenGlobalPlace::init()
{
  initPlacerBase();
  // FIXME: initial placement must be based on weighted nets in order to
  // ignore scan enables and scan inputs.
  doInitialPlace();
  FASSERT(!linden_db_);
  linden_db_ = create_linden_db();

  FASSERT(!lindengp_);
  {
    linden::gpl::PlacerConfig::PlacerType ty
        = linden::gpl::PlacerConfig::NPLACE;
    linden::gpl::PlacerConfig cfg(ty);
    cfg.target_density = density_;
    cfg.target_overflow = overflow_;
    cfg.use_uniform_density = use_uniform_density_;
    cfg.zigzag = zigzag_;
    cfg.cgr_mode = cgr_mode_;
    cfg.cgr_params_file = ord::OpenRoad::openRoad()->getCongestionRepair()->getCGRParamsFile();
    cfg.routability_driven = routability_driven_;
    {
      linden::dp::LefDefDumper dumper;
      dumper.run(*ord::OpenRoad::openRoad());
      cfg.input_def = dumper.def();
      cfg.tech_lef = dumper.tech_lef();
      cfg.lib_lefs = dumper.lib_lefs();
    }
    lindengp_ = linden::gpl::Placer::create(linden_db_, cfg, log_);
  }
}

void LindenGlobalPlace::change_to_cgr_mode()
{
  routability_driven_ = true;
  initialPlacePolicy_ = "def";

  // disable gp-dp zigzag mode here
  zigzag_ = false;
  cgr_mode_ = true;

  {  // FIXME: is it necessary to load the following from pb file?
    density_ = 1.0;
    overflow_ = 0.2;
    use_uniform_density_ = true;
  }
}

void LindenGlobalPlace::run()
{
  init();
  FASSERT(!banyan_cmap_result_);
  if (routability_driven_) {
    banyan_cmap_result_ = std::make_shared<linden::rdp::CongestionMap>(log_);
  }
  std::unique_ptr<Graphics> graphics;
  if (Graphics::guiActive()) {
    graphics
        = make_unique<Graphics>(log_, linden_db_, banyan_cmap_result_, pb_);
  }
  const int MAX_ITER = 5000;
  if (!timing_overflows_.empty()) {
    rsz::Resizer* rs_ = ::ord::OpenRoad::openRoad()->getResizer();
    rs_->resizeSlackPreamble();
  }
  size_t iter = 0;
  for (; iter < MAX_ITER; iter++) {
    auto ret = lindengp_->one_step_move(iter);
    if (!ret) {
      break;
    }
    auto overflow = *ret;
    if (routability_driven_) {
      bool res = rdp_cmap(overflow);
      if (!res) {
        routability_driven_ = false;
        break;
      }
    } else if (!zigzag_ && overflow <= overflow_) {
      break;
    }

    if (graphics) {
      bool update
          = (iter == 0 || (iter + 1) % gui_debug_update_iterations_ == 0);
      bool pause = (iter == 0 || (iter + 1) % gui_debug_pause_iterations_ == 0);
      if (banyan_cmap_result_ && !banyan_cmap_result_->empty()) {
        update = (iter + 1) % 10 == 0;
        pause = true;  // pause for zoom-in: debug and analyze
                       // CongestionEstimation|Repair
      }
      if (update) {
        graphics->cellPlot(pause);
      }
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
  log_->metric("gp::iter", iter + 1);
  
  lindengp_->done();

  if (graphics) {
    graphics->status("End placement");
    graphics->cellPlot(true);
  }

  update_db();
  report_metrics();
  reset();
}

double LindenGlobalPlace::cgr_run()
{
  change_to_cgr_mode();
  FASSERT(routability_driven_);
  banyan_cmap_result_ = std::make_shared<linden::rdp::CongestionMap>(log_);
  congestion_estimate();
  const auto congestion = banyan_cmap_result_->getCurRouteCongestionValue();
  if (congestion != 0) {
    init();
    std::unique_ptr<Graphics> graphics;
    if (Graphics::guiActive()) {
      graphics
          = make_unique<Graphics>(log_, linden_db_, banyan_cmap_result_, pb_);
    }
    if (lindengp_->load_cmap(*banyan_cmap_result_)) {
      const int MAX_ITER = 50;
      for (size_t iter = 0; iter < MAX_ITER; iter++) {
        auto ret = lindengp_->one_step_move(iter);
        if (!ret) {
          break;
        }
        if (*ret < 0.15 && iter > 10) {
          break;
        }
        if (graphics) {
          bool update
              = (iter == 0 || (iter + 1) % gui_debug_update_iterations_ == 0);
          bool pause
              = (iter == 0 || (iter + 1) % gui_debug_pause_iterations_ == 0);
          if (banyan_cmap_result_ && !banyan_cmap_result_->empty()) {
            update = (iter + 1) % 10 == 0;
            pause = true;  // pause for zoom-in: debug and analyze
                           // CongestionEstimation|Repair
          }
          if (update) {
            graphics->cellPlot(pause);
          }
        }
      }
      if (graphics) {
        graphics->status("End cgr placement");
        graphics->cellPlot(true);
      }
      
      lindengp_->done();
      update_db();
    }
  }
  reset();
  return congestion;
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
