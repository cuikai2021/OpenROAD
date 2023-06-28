#include "linden/ignored_nets.hpp"

#include <deque>

#include "envflag/envflag.hpp"
#include "linden/scanchain.hpp"
#include "odb/db.h"

using namespace std;
using namespace odb;

IMPORT_F64_FLAG_WITH_DEFAULT(LINDEN_IGNORED_NETS_MAX_FANOUT);
IMPORT_F64_FLAG_WITH_DEFAULT(LINDEN_SCAN_ENABLE_WEIGHT);
IMPORT_F64_FLAG_WITH_DEFAULT(LINDEN_CLOCK_WEIGHT);
IMPORT_F64_FLAG_WITH_DEFAULT(LINDEN_RESET_WEIGHT);

namespace {

// only for internal use

unordered_set<odb::dbNet*> nets_to_scanchain_enables()
{
  deque<linden::PairedCell> cells_on_sc = linden::cells_on_scanchains();
  unordered_set<odb::dbNet*> res;
  for (auto const& cell : cells_on_sc) {
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

unordered_set<odb::dbNet*> nets_to_clock(
    const deque<linden::PairedCell>& all_cells)
{
  unordered_set<odb::dbNet*> res;
  for (const linden::PairedCell& cell : all_cells) {
    for (const linden::PairedPin& pin : cell.clocks()) {
      odb::dbNet* net = pin.db().getNet();
      if (net != nullptr) {
        res.insert(net);
      }
    }
  }
  return res;
}

unordered_set<odb::dbNet*> nets_to_resets(
    const deque<linden::PairedCell>& all_cells)
{
  unordered_set<odb::dbNet*> res;
  for (const linden::PairedCell& cell : all_cells) {
    for (const linden::PairedPin& pin : cell.resets()) {
      odb::dbNet* net = pin.db().getNet();
      if (net != nullptr) {
        res.insert(net);
      }
    }
  }
  return res;
}

}  // namespace

namespace gpl {

IsScanChainsIgnoredPin::IsScanChainsIgnoredPin(
    const odb::dbSet<odb::dbScanChain>& scan_chains)
{
  for (auto scan_chain : scan_chains) {
    for (auto floating_comp : scan_chain->getFloating()) {
      auto in_iterm = floating_comp->getScanIn();
      sc_iterms.insert(in_iterm);
    }
    for (auto ordered_comps : scan_chain->getOrdered()) {
      auto in_iterm = ordered_comps->getComps().begin()->getScanIn();
      sc_iterms.insert(in_iterm);
    }
    auto stop_iterm = scan_chain->getStop();
    sc_iterms.insert(stop_iterm);
  }
}

bool IsScanChainsIgnoredPin::operator()(const odb::dbITerm* iterm_pin) const
{
  return sc_iterms.find(iterm_pin) != sc_iterms.end();
}

IgnoredNets::IgnoredNets()
{
  init();
}

void IgnoredNets::init() noexcept
{
  const deque<linden::PairedCell>& all_cells_w_lib
      = linden::all_cells_with_lib();
  clock_nets_ = nets_to_clock(all_cells_w_lib);
  reset_nets_ = nets_to_resets(all_cells_w_lib);
  scan_enable_nets_ = nets_to_scanchain_enables();
}

bool IgnoredNets::is_clock_net(odb::dbNet* net) const noexcept
{
  return clock_nets_.find(net) != clock_nets_.end();
}
bool IgnoredNets::is_reset_net(odb::dbNet* net) const noexcept
{
  return reset_nets_.find(net) != reset_nets_.end();
}

bool IgnoredNets::is_scan_enable_net(odb::dbNet* net) const noexcept
{
  return scan_enable_nets_.find(net) != scan_enable_nets_.end();
}

double IgnoredNets::get_net_weight(odb::dbNet* net) const noexcept
{
  if (is_scan_enable_net(net)) {
    return LINDEN_SCAN_ENABLE_WEIGHT();
  }
  if (is_clock_net(net)) {
    return LINDEN_CLOCK_WEIGHT();
  }
  if (is_reset_net(net)) {
    return LINDEN_RESET_WEIGHT();
  }
  return 1.0;
}

bool is_high_fanout_net(odb::dbNet* net) noexcept
{
  dbSigType netType = net->getSigType();
  int64_t pin_cnt = 0;
  if (netType == dbSigType::SIGNAL || netType == dbSigType::CLOCK) {
    pin_cnt += net->getITerms().size();
    pin_cnt += net->getBTerms().size();
  }
  return pin_cnt >= LINDEN_IGNORED_NETS_MAX_FANOUT();
}

}  // namespace gpl
