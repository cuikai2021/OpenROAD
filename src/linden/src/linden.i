%{
#include "linden/gp.hpp"
#include "./mp/lindenmp.hpp"
#include "./itdp/ffi.hpp"
#include "./timing_data/ffi.hpp"
#include "./dp/ffi.hpp"
#include "./gp/ffi.hpp"
#include "./remove_fillers/ffi.hpp"
#include <memory>
#include <deque>
#include <string>
%}

%inline %{

class StrDeque {
public:
    explicit StrDeque() {}
    void push_back(const char* elem) {
        elems_.emplace_back(elem);
    }

    std::deque<std::string> elems_;
};

%}

%inline %{ // lindengp

void set_density(float density)
{
    linden::gp::Config::get_default()->mut_density() = density;
}

void set_to_uniform_density()
{
    linden::gp::Config::get_default()->set_to_uniform();
}

void set_overflow(float overflow)
{
    linden::gp::Config::get_default()->overflow = overflow;
}

void set_linden_pad_left_cmd(int pad)
{
    // linden::gp::Config::get_default()->left_pad = pad;
}

void set_linden_pad_right_cmd(int pad)
{
    // linden::gp::Config::get_default()->right_pad = pad;
}

void enable_timing_driven_mode()
{
    linden::gp::Config::get_default()->timing_driven = true;
}

void set_initial_place_policy(const char* policy)
{
    using linden::gp::Config;
    Config::get_default()->initial_place_policy =
        Config::InitialPlacePolicy::from_str(policy);
}

void lindengp_run_cmd(StrDeque& args)
{
    linden::gp::gogogo(std::move(args.elems_));
}

void set_linden_debug_cmd(int pause_iterations,
              int update_iterations,
              bool initial)
{
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->setDebug(pause_iterations, update_iterations, initial);
}

void set_linden_routability_driven_mode(bool flag) {
// FIXME:
//   LindenGlobalPlace* lindengp = getLindenGlobalPlace();
//   lindengp->setRoutabilityDriven(flag);
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
    linden::remove_filler::gogogo();
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
    linden::itdp::itdp_gogogo(pb_path);
}

%} // inline linden_lpbased_postdrv_itdp

%inline %{

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

%inline %{

double run()
{
// cgr::CongestionRepair* cgr = getCongestionRepair();
// return cgr->run();
    return 0;
}

void done() {
// cgr::CongestionRepair* cgr = getCongestionRepair();
// return cgr->done();
}

void set_cgr_params_file_cmd(const char* cgr_params_file) {
// cgr::CongestionRepair* cgr = getCongestionRepair();
// cgr->setCGRParamsFile(cgr_params_file);
}

void set_routing_max_layer_cmd(const char* max_layer) {
// cgr::CongestionRepair* cgr = getCongestionRepair();
// cgr->setRoutingMaxLayer(max_layer);
}

void analyze_banyan_cmap_file_cmd(const char* cmap_file, const char* report_file) {
// cgr::analyze_banyan_cmap_file(cmap_file, report_file);
}

%} // cgr
