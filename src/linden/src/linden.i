%{
#include "linden/lindengp.hpp"
#include "linden/lindenmp.hpp"
#include "linden/linden_lpbased_postdrv_itdp.hpp"
#include "linden/db_sanitizer.hpp"
#include "./timing_data/ffi.hpp"
#include "./dp/ffi.hpp"
#include <memory>
#include <deque>
#include <string>

namespace ord {
OpenRoad* getOpenRoad();

// gpl::LindenGlobalPlace* getLindenGlobalPlace();

// std::shared_ptr<lindenmp::LindenMacroPlace> getLindenMacroPlace();
}

// using ord::getOpenRoad;

// using ord::getLindenGlobalPlace;
// using gpl::LindenGlobalPlace;

%}

%inline %{ // lindengp

void lindengp_run_cmd()
{
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->run();
}

void set_linden_density_cmd(float density)
{
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->setTargetDensity(density);
}

void set_linden_overflow_cmd(float overflow)
{
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->setTargetOverflow(overflow);
}

void set_linden_use_uniform_density_cmd(bool use_uniform_density)
{
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->setUniformDensity(use_uniform_density);
}

void set_linden_pad_left_cmd(int pad)
{
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->setPadLeft(pad);
}

void set_linden_pad_right_cmd(int pad)
{
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->setPadRight(pad);
}

void set_linden_debug_cmd(int pause_iterations,
              int update_iterations,
              bool initial)
{
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->setDebug(pause_iterations, update_iterations, initial);
}

void set_linden_timing_driven_mode(bool flag)
{
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->setTimingDriven(flag);
}

void set_linden_routability_driven_mode(bool flag) {
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->setRoutabilityDriven(flag);
}

void set_linden_initial_place_policy_cmd(const char* policy) {
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->setInitialPlacePolicy(policy);
}

void set_linden_focus_insts_cmd(const char* focus_insts_file_name) {
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->setFocusInsts(focus_insts_file_name);
}

void set_linden_focus_nets_cmd(const char* focus_nets_file_name) {
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->setFocusNets(focus_nets_file_name);
}

void enable_linden_continuation() {
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->enableContinuation();
}

%} // inline lindengp

%inline %{ // linden others
void linden_remove_fillers_cmd() {
// FIXME:
//   linden::LindenDBSanitizer *sanitizer = new linden::LindenDBSanitizer();
//   sanitizer->removeFillers();
}
%} // linden others

%inline %{ // lindenmp

int lindenmp_run_cmd()
{
// FIXME:
//   auto lmp = ord::getLindenMacroPlace();
//   return lmp->run();
    return 0;
}

void set_lindenmp_density_cmd(double density)
{
// FIXME:
//   auto lmp = ord::getLindenMacroPlace();
//   lmp->mut_density() = density;
}

void set_lindenmp_overflow_cmd(double overflow)
{
// FIXME:
//   auto lmp = ord::getLindenMacroPlace();
//   lmp->mut_overflow() = overflow;
}

void set_lindenmp_use_uniform_density_cmd(double use_uniform_density)
{
// FIXME:
//   auto lmp = ord::getLindenMacroPlace();
//   lmp->mut_use_uniform_density() = use_uniform_density;
}

void set_lindenmp_debug(int pause_iterations, int update_iterations)
{
// FIXME:
//   auto lmp = ord::getLindenMacroPlace();
//   lmp->mut_debug_pause_iters_() = pause_iterations;
//   lmp->mut_debug_update_iters_() = update_iterations;
}

%} // inline lindenmp

%inline %{ // linden_lpbased_postdrv_itdp

void linden_lpbased_postdrv_itdp(const char* pb_path)
{
// FIXME:
//   itdp::LPBasedPostDrvItdp ldnx;
//   ldnx.run(pb_path);
}

%} // inline linden_lpbased_postdrv_itdp

%inline %{

class StrDeque {
public:
    explicit StrDeque() {}
    void push_back(const char* elem) {
        elems_.emplace_back(elem);
    }

    std::deque<std::string> elems_;
};

int lindendp_gogogo(StrDeque& args) {
    return linden::basic_dp_gogogo(std::move(args.elems_));
}

int linden_incr_legalize(StrDeque& args) {
    return linden::incr_dp_gogogo(std::move(args.elems_));
}

%} // lindendp

%inline %{

void write_tdp_data_cmd(const char* filename) {
    linden::write_tdp_data(filename);
}

void read_tdp_data_cmd(const char* filename) {
    linden::read_tdp_data(filename);
}

%} // timing data