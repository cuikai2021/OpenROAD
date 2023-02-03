%{
#include "ord/OpenRoad.hh"
#include "gpl/Replace.h"
#include "gpl/lindengp.h"
#include "gpl/lindenmp.hpp"
#include "gpl/LindenDBSanitizer.hpp"
#include <memory>

namespace ord {
OpenRoad*
getOpenRoad();

gpl::Replace*
getReplace();

gpl::LindenGlobalPlace*
getLindenGlobalPlace();

std::shared_ptr<lindenmp::LindenMacroPlace> getLindenMacroPlace();
}

using ord::getOpenRoad;
using ord::getReplace;
using gpl::Replace;

using ord::getLindenGlobalPlace;
using gpl::LindenGlobalPlace;

%}

%include "../../Exception.i"

%inline %{ // replace

void
replace_reset_cmd()
{
  Replace* replace = getReplace();
  replace->reset();
  replace->setDb(getOpenRoad()->getDb());
  replace->setLogger(getOpenRoad()->getLogger());
  replace->setGlobalRouter(getOpenRoad()->getGlobalRouter());
  replace->setResizer(getOpenRoad()->getResizer());
}

void
replace_initial_place_cmd()
{
  Replace* replace = getReplace();
  replace->doInitialPlace();
}

void
replace_nesterov_place_cmd()
{
  Replace* replace = getReplace();
  replace->doNesterovPlace();
}

void
set_density_cmd(float density)
{
  Replace* replace = getReplace();
  replace->setTargetDensity(density);
}

void
set_uniform_target_density_mode_cmd(bool uniform)
{
  Replace* replace = getReplace();
  replace->setUniformTargetDensityMode(uniform);
}

void
set_initial_place_guide_file_cmd(const char* path) {
  Replace* replace = getReplace();
  replace->setInitialPlaceGuideFile(path);
}

void
set_initial_place_policy_cmd(const char* policy) {
  Replace* replace = getReplace();
  replace->setInitialPlacePolicy(policy);
}

void
set_cell_density_force_policy_cmd(const char* policy) {
  Replace* replace = getReplace();
  replace->setCellDensityForceCalPolicy(policy);
}

void
set_initial_place_max_iter_cmd(int iter)
{
  Replace* replace = getReplace();
  replace->setInitialPlaceMaxIter(iter);
}

void
set_initial_place_max_fanout_cmd(int fanout)
{
  Replace* replace = getReplace();
  replace->setInitialPlaceMaxFanout(fanout);
}

void
set_nesv_place_iter_cmd(int iter)
{
  Replace* replace = getReplace();
  replace->setNesterovPlaceMaxIter(iter);
}

void
set_bin_grid_cnt_x_cmd(int cnt_x)
{
  Replace* replace = getReplace();
  replace->setBinGridCntX(cnt_x);
}

void
set_bin_grid_cnt_y_cmd(int cnt_y)
{
  Replace* replace = getReplace();
  replace->setBinGridCntY(cnt_y);
}

void
set_overflow_cmd(float overflow)
{
  Replace* replace = getReplace();
  replace->setTargetOverflow(overflow);
}

void
set_min_phi_coef_cmd(float min_phi_coef)
{
  Replace* replace = getReplace();
  replace->setMinPhiCoef(min_phi_coef);
}

void
set_max_phi_coef_cmd(float max_phi_coef)
{
  Replace* replace = getReplace();
  replace->setMaxPhiCoef(max_phi_coef);
}

void
set_reference_hpwl_cmd(float reference_hpwl)
{
  Replace* replace = getReplace();
  replace->setReferenceHpwl(reference_hpwl);
}

void
set_init_density_penalty_factor_cmd(float penaltyFactor)
{
  Replace* replace = getReplace();
  replace->setInitDensityPenalityFactor(penaltyFactor);
}

void
set_init_wirelength_coef_cmd(float coef)
{
  Replace* replace = getReplace();
  replace->setInitWireLengthCoef(coef);
}

void
replace_incremental_place_cmd()
{
  Replace* replace = getReplace();
  replace->doIncrementalPlace();
}

void
set_timing_driven_mode(bool timing_driven)
{
  Replace* replace = getReplace();
  replace->setTimingDrivenMode(timing_driven);
}

void
set_routability_driven_mode(bool routability_driven)
{
  Replace* replace = getReplace();
  replace->setRoutabilityDrivenMode(routability_driven);
}

void
set_routability_check_overflow_cmd(float overflow)
{
  Replace* replace = getReplace();
  replace->setRoutabilityCheckOverflow(overflow);
}

void
set_routability_max_density_cmd(float density)
{
  Replace* replace = getReplace();
  replace->setRoutabilityMaxDensity(density);
}

void
set_routability_max_bloat_iter_cmd(int iter)
{
  Replace* replace = getReplace();
  replace->setRoutabilityMaxBloatIter(iter);
}

void
set_routability_max_inflation_iter_cmd(int iter)
{
  Replace* replace = getReplace();
  replace->setRoutabilityMaxInflationIter(iter);
}

void
set_routability_target_rc_metric_cmd(float rc)
{
  Replace* replace = getReplace();
  replace->setRoutabilityTargetRcMetric(rc);
}

void
set_routability_inflation_ratio_coef_cmd(float coef)
{
  Replace* replace = getReplace();
  replace->setRoutabilityInflationRatioCoef(coef);
}

void
set_routability_max_inflation_ratio_cmd(float ratio)
{
  Replace* replace = getReplace();
  replace->setRoutabilityMaxInflationRatio(ratio);
}

void
set_routability_rc_coefficients_cmd(float k1,
                                    float k2,
                                    float k3,
                                    float k4)
{
  Replace* replace = getReplace();
  replace->setRoutabilityRcCoefficients(k1, k2, k3, k4);
}


void
set_pad_left_cmd(int pad)
{
  Replace* replace = getReplace();
  replace->setPadLeft(pad);
}

void
set_pad_right_cmd(int pad)
{
  Replace* replace = getReplace();
  replace->setPadRight(pad);
}

void
set_skip_io_mode_cmd(bool mode)
{
  Replace* replace = getReplace();
  replace->setSkipIoMode(mode);
}

void
set_disable_filler_insertion_cmd(bool mode)
{
  Replace* replace = getReplace();
  replace->setDisableFillerInsertion(mode);
}

void
set_disable_cell_density_scaling_cmd(bool mode)
{
  Replace* replace = getReplace();
  replace->setDisableCellDensityScaling(mode);
}

float
get_global_placement_uniform_density_cmd()
{
  Replace* replace = getReplace();
  return replace->getUniformTargetDensity();
}

void
add_timing_net_reweight_overflow_cmd(int overflow)
{
  Replace* replace = getReplace();
  return replace->addTimingNetWeightOverflow(overflow);
}

void
set_debug_cmd(int pause_iterations,
              int update_iterations,
              bool draw_bins,
              bool initial)
{
  Replace* replace = getReplace();
  replace->setDebug(pause_iterations, update_iterations, draw_bins,
                    initial);
}

void
set_plot_path_cmd(const char* path)
{
  Replace* replace = getReplace();
  replace->setPlottingPath(path);
}

%} // inline replace

%inline %{ // lindengp

void
lindengp_run_cmd()
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->run();
}

void
set_linden_density_cmd(float density)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setTargetDensity(density);
}

void
set_linden_overflow_cmd(float overflow)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setTargetOverflow(overflow);
}

void
set_linden_use_uniform_density_cmd(bool use_uniform_density)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setUniformDensity(use_uniform_density);
}

void
set_linden_zigzag_cmd(bool zigzag)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setZigzag(zigzag);
}

void
set_linden_pad_left_cmd(int pad)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setPadLeft(pad);
}

void
set_linden_pad_right_cmd(int pad)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setPadRight(pad);
}

void
set_linden_debug_cmd(int pause_iterations,
              int update_iterations,
              bool initial)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setDebug(pause_iterations, update_iterations, initial);
}

void
set_linden_timing_driven_mode(bool timing_driven)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setTimingDriven(timing_driven);
}

void
set_linden_initial_place_policy_cmd(const char* policy) {
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setInitialPlacePolicy(policy);
}

%} // inline lindengp

%inline %{ // linden
void 
linden_remove_fillers_cmd() {
  linden::LindenDBSanitizer *sanitizer = new linden::LindenDBSanitizer();
  sanitizer->removeFillers();
}
%} // inline linden

%inline %{ // lindenmp

int lindenmp_run_cmd()
{
  auto lmp = ord::getLindenMacroPlace();
  return lmp->run();
}

void set_lindenmp_density_cmd(double density)
{
  auto lmp = ord::getLindenMacroPlace();
  lmp->mut_density() = density;
}

void set_lindenmp_overflow_cmd(double overflow)
{
  auto lmp = ord::getLindenMacroPlace();
  lmp->mut_overflow() = overflow;
}

void set_lindenmp_use_uniform_density_cmd(double use_uniform_density)
{
  auto lmp = ord::getLindenMacroPlace();
  lmp->mut_use_uniform_density() = use_uniform_density;
}

void set_lindenmp_debug(int pause_iterations, int update_iterations)
{
  auto lmp = ord::getLindenMacroPlace();
  lmp->mut_debug_pause_iters_() = pause_iterations;
  lmp->mut_debug_update_iters_() = update_iterations;
}



%} // inline lindenmp
