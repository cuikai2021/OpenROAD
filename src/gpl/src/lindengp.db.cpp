#include "gpl/lindengp.h"

#include <unordered_set>

#include "linden/scanchain.hpp"
#include "odb/db.h"
#include "placerBase.h"
#include "utl/Logger.h"
#include "linden/ignored_nets.hpp"

#include "fassert.hpp"

using namespace std;
using utl::GPL;

namespace gpl {

std::shared_ptr<linden::db::Database> LindenGlobalPlace::create_linden_db()
{
  FASSERT(pb_ != nullptr);
  auto const& die = pb_->die();
  linden::geom::Rect die_region(
      die.dieLx(), die.dieLy(), die.dieUx(), die.dieUy());
  linden::geom::Rect core_region(
      die.coreLx(), die.coreLy(), die.coreUx(), die.coreUy());

  IgnoredNets ignored_nets;

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

  auto se_nets = ignored_nets.scan_enable_nets();
  debugPrint(log_,
             GPL,
             "linden",
             1,
             "#nets connecting scan enables: {}",
             se_nets.size());
  auto clock_nets = ignored_nets.clock_nets();
  debugPrint(
      log_, GPL, "linden", 1, "#nets connecting clocks: {}", clock_nets.size());
  auto reset_nets = ignored_nets.reset_nets();
  debugPrint(
      log_, GPL, "linden", 1, "#nets connecting resets: {}", reset_nets.size());
  for (auto& net : pb_->nets()) {
    linden::db::Net lnet((void*) net->dbNet(), net->dbNet()->getName());
    lnet.mut_weight() = ignored_nets.get_net_weight(net->dbNet());
    linden_db->add_net(std::move(lnet));
  }
  IsScanChainsIgnoredPin isScanChainsIgnoredPin(
      pb_->db()->getChip()->getBlock()->getScanChains());
  for (auto& pin : pb_->pins()) {
    if (pin->isITerm() && isScanChainsIgnoredPin(pin->dbITerm())) {
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