#include "gpl/lindengp.h"

#include <unordered_set>

#include "envflag/envflag.hpp"
#include "linden/scanchain.hpp"
#include "odb/db.h"
#include "placerBase.h"
#include "utl/Logger.h"

IMPORT_F64_FLAG_WITH_DEFAULT(LINDEN_SCAN_ENABLE_WEIGHT);
IMPORT_F64_FLAG_WITH_DEFAULT(LINDEN_CLOCK_WEIGHT);
IMPORT_F64_FLAG_WITH_DEFAULT(LINDEN_RESET_WEIGHT);

using namespace std;
using utl::GPL;

namespace gpl {

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
  debugPrint(log_,
             GPL,
             "linden",
             1,
             "#nets connecting scan enables: {}",
             se_nets.size());
  const deque<linden::PairedCell>& all_cells_w_lib
      = linden::all_cells_with_lib();
  const unordered_set<odb::dbNet*>& clock_nets = nets_to_clock(all_cells_w_lib);
  debugPrint(
      log_, GPL, "linden", 1, "#nets connecting clocks: {}", clock_nets.size());
  const unordered_set<odb::dbNet*>& reset_nets
      = nets_to_resets(all_cells_w_lib);
  debugPrint(
      log_, GPL, "linden", 1, "#nets connecting resets: {}", reset_nets.size());
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

}  // namespace gpl