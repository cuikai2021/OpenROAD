%{
#include "linden/linden.hpp"
#include "ord/OpenRoad.hh"
#include "gpl/lindengp.h"
#include "gpl/lindenmp.hpp"
#include "linden/linden_lpbased_postdrv_itdp.hpp"
#include "linden/db_sanitizer.hpp"
#include <memory>
#include <deque>
#include <string>

namespace ord {
OpenRoad* getOpenRoad();

gpl::LindenGlobalPlace* getLindenGlobalPlace();

std::shared_ptr<lindenmp::LindenMacroPlace> getLindenMacroPlace();
}

using ord::getOpenRoad;

using ord::getLindenGlobalPlace;
using gpl::LindenGlobalPlace;

%}

%include "../../Exception.i"

%inline %{ // lindengp

void lindengp_run_cmd()
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->run();
}

void set_linden_density_cmd(float density)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setTargetDensity(density);
}

void set_linden_overflow_cmd(float overflow)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setTargetOverflow(overflow);
}

void set_linden_use_uniform_density_cmd(bool use_uniform_density)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setUniformDensity(use_uniform_density);
}

void set_linden_zigzag_cmd(bool zigzag)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setZigzag(zigzag);
}

void set_linden_pad_left_cmd(int pad)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setPadLeft(pad);
}

void set_linden_pad_right_cmd(int pad)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setPadRight(pad);
}

void set_linden_debug_cmd(int pause_iterations,
              int update_iterations,
              bool initial)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setDebug(pause_iterations, update_iterations, initial);
}

void set_linden_timing_driven_mode(bool flag)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setTimingDriven(flag);
}

void set_linden_routability_driven_mode(bool flag)
{
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setRoutabilityDriven(flag);
}

void set_linden_initial_place_policy_cmd(const char* policy) {
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setInitialPlacePolicy(policy);
}

void set_linden_focus_insts_cmd(const char* focus_insts_file_name) {
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setFocusInsts(focus_insts_file_name);
}

void set_linden_focus_nets_cmd(const char* focus_nets_file_name) {
  LindenGlobalPlace* lindengp = getLindenGlobalPlace();
  lindengp->setFocusNets(focus_nets_file_name);
}

%} // inline lindengp

%inline %{ // linden
void linden_remove_fillers_cmd() {
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

%inline %{ // linden_lpbased_postdrv_itdp

void linden_lpbased_postdrv_itdp(const char* pb_path)
{
  itdp::LPBasedPostDrvItdp ldnx;
  ldnx.run(pb_path);
}

%} // inline linden_lpbased_postdrv_itdp

%inline %{

namespace linden {

class StrDeque {
public:
    explicit StrDeque() {}
    void push_back(const char* elem) {
        elems_.emplace_back(elem);
    }

    std::deque<std::string> elems_;
};

int lindendp_gogogo(StrDeque& args) {
    ord::OpenRoad* ord = ord::OpenRoad::openRoad();
    DetailedPlacer dp(*ord);
    return dp.go(args.elems_);
}

int linden_incr_legalize(StrDeque& args) {
    ord::OpenRoad* ord = ord::OpenRoad::openRoad();
    IncrLegalizer leg(*ord);
    return leg.go(args.elems_);

}

} // namespace linden

%}
