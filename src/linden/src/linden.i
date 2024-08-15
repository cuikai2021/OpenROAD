%{
#include "linden/gp.hpp"
#include "./cgr/ffi.hpp"
#include "./dp/ffi.hpp"
#include "./gp/ffi.hpp"
#include "./initial_place/ffi.hpp"
#include "./itdp/ffi.hpp"
#include "./mp/ffi.hpp"
#include "./remove_fillers/ffi.hpp"
#include "./timing_data/ffi.hpp"
#include <memory>
#include <deque>
#include <string>
#include "envflag/envflag.hpp"
IMPORT_STR_FLAG_WITH_DEFAULT(LINDEN_TMP_FILE_ROOT_DIR);
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

void enable_routability_driven_mode() {
    linden::gp::Config::get_default()->routability_driven = true;
}

void set_overflow_to_estimate_cmap(float overflow) {
    linden::gp::Config::get_default()->overflow_to_estimate_cmap = overflow;
}

void set_highest_layer_to_route(const char* highest_layer) {
    linden::gp::Config::get_default()->mut_highest_layer_to_route() = highest_layer;
}

void lindengp_run_cmd(StrDeque& args)
{
    linden::gp::gogogo(std::move(args.elems_));
}

void linden_initial_place()
{
    auto cfg = linden::gp::Config::get_default();
    linden::ip::gogogo(*cfg);
}

void set_continuation(const char* file) {
    using linden::gp::Config;
    Config::get_default()->continuation = file;
}

void lindengp_random_run_cmd() {
    linden::gp::random_place();
}


%} // inline lindengp

%inline %{ // linden others
void linden_remove_fillers_cmd() {
    linden::remove_filler::gogogo();
}
%} // linden others

%inline %{ // lindenmp

void lindenmp_run_cmd(StrDeque& args)
{
    linden::mp::mp_gogogo(std::move(args.elems_));
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

int routability_driven_legalize(StrDeque& args) {
    return linden::rddp_gogogo(std::move(args.elems_));
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

int analyze_banyan_cmap_file_cmd(const char* cmap_file, const char* report_file) {
    return linden::cgr::analyze_file_gogogo(cmap_file, report_file);
}

%} // cgr

%inline %{

const char* env_tmp_file_root_dir() {
    return LINDEN_TMP_FILE_ROOT_DIR().data();
}

%} // exposing envflags