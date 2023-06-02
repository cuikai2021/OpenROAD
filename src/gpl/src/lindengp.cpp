#include "gpl/lindengp.h"

#include "cgr/congestionRepair.hpp"
#include "envflag/envflag.hpp"
#include "fassert.hpp"
#include "graphics.h"
#include "linden/gpl/placer_config.hpp"
#include "linden/lefdef_dumper.hpp"
#include "linden/relevance_verify.hpp"
#include "ord/OpenRoad.hh"
#include "placerBase.h"
#include "rsz/Resizer.hh"
#include "utl/Logger.h"
#include "utl/metrics.hpp"

using namespace std;
using utl::GPL;

IMPORT_STR_FLAG_WITH_DEFAULT(LINDEN_GP_METRICS_FILE);
IMPORT_BOOL_FLAG_WITH_DEFAULT(LINDEN_GP_CELL_RELATIVE_POS_CAL);

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
    cfg.cgr_params_file
        = ord::OpenRoad::openRoad()->getCongestionRepair()->getCGRParamsFile();
    cfg.routability_driven = routability_driven_;
    {
      linden::dp::LefDefDumper dumper;
      dumper.run(*ord::OpenRoad::openRoad());
      cfg.input_def = dumper.def();
      cfg.tech_lef = dumper.tech_lef();
      cfg.lib_lefs = dumper.lib_lefs();
    }
    lindengp_ = linden::gpl::Placer::create(linden_db_, cfg);
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
    banyan_cmap_result_ = std::make_shared<linden::rdp::CongestionMap>();
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
  relevance::RelevanceDetector relev(*linden_db_);
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
    if (LINDEN_GP_CELL_RELATIVE_POS_CAL()) {
      if (iter % 10 == 0) {
        auto const& [x_change, y_change] = relev.update(*linden_db_);
        log_->info(GPL,
                   1001,
                   "NPlace: relative position change = ({:.2f}%, {:.2f}%) ",
                   x_change * 100,
                   y_change * 100);
      }
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
  metrics::Config mcfg = metrics::Config::sink_to_stderr();
  {
    string_view filename = LINDEN_GP_METRICS_FILE();
    if (!filename.empty()) {
      mcfg = metrics::Config::sink_to_file(filename);
    }
  }
  metrics::Reporter metrics_reporter(mcfg);
  metrics_reporter["gp::iter"] = iter + 1;

  lindengp_->done();

  if (graphics) {
    graphics->status("End placement");
    graphics->cellPlot(true);
  }

  update_db();
  report_metrics(metrics_reporter);
  reset();
}

double LindenGlobalPlace::cgr_run()
{
  change_to_cgr_mode();
  FASSERT(routability_driven_);
  banyan_cmap_result_ = std::make_shared<linden::rdp::CongestionMap>();
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

}  // namespace gpl
