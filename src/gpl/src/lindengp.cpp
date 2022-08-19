#include "gpl/lindengp.h"
#include "ord/OpenRoad.hh"

#include <iostream>

#include "graphics.h"
#include "linden/gpl/placer_config.hpp"
#include "linden/lefdef_dumper.hpp"
#include "odb/db.h"
#include "placerBase.h"
#include "utl/Logger.h"

using namespace std;
using utl::GPL;

namespace gpl {

LindenGlobalPlace::LindenGlobalPlace()
  : db_(nullptr),
    log_(nullptr),
    pb_(nullptr),
    linden_db_(nullptr),
    lindengp_(nullptr),
    density_(1.0),
    overflow_(0.1),
    use_uniform_density_(false),
    padLeft_(0),
    padRight_(0),
    gui_debug_(false),
    gui_debug_pause_iterations_(10),
    gui_debug_update_iterations_(10) {
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

std::shared_ptr<linden::db::Database>
LindenGlobalPlace::create_linden_db()
{
  initPlacerBase();
  auto const& die = pb_->die();
  linden::geom::Rect core_region(die.coreLx(), die.coreLy(), die.coreUx(), die.coreUy());

  linden::db::Database::Config cfg = {
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
    const linden::geom::Rect rect(
        inst->lx(), inst->ly(), inst->ux(), inst->uy());
    int64_t inst_area = inst->area();
    bool is_macro = inst->isMacro();
    bool is_fixed = inst->isFixed();
    if (inst->dbInst() != nullptr) {
      void* odb_instance = inst->dbInst();
      linden_db->add_cell(
          linden::db::Cell(odb_instance, inst->dbInst()->getName(), rect, inst_area, is_macro, is_fixed));
    } else {
      linden_db->add_cell(linden::db::Cell(rect, is_fixed));
    }
  }
  for (auto& net : pb_->nets()) {
    linden_db->add_net(linden::db::Net((void*) net->dbNet(), net->dbNet()->getName()));
  }
  for (auto& pin : pb_->pins()) {
    auto o_net = linden_db->odb2net(pin->net()->dbNet());
    assert(o_net);
    if (pin->instance()) {
      assert(pin->isITerm());
      auto o_cell = linden_db->odb2cell(pin->instance()->dbInst());
      assert(o_cell);
      void* term = pin->dbITerm();
      std::string name = pin->dbITerm()->getMTerm()->getName();
      linden::geom::Point pin_offset(pin->offsetCx(), pin->offsetCy());
      linden_db->add_pin(linden::db::ITerm(term, *o_net, name, *o_cell, pin_offset));
    } else {
      void* term = nullptr;
      std::string name = "";
      if(pin->isITerm()) {
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

void LindenGlobalPlace::run()
{
  initPlacerBase();
  if (!linden_db_) {
    linden_db_ = create_linden_db();
  }
  if (!lindengp_) {
    linden::gpl::PlacerConfig::PlacerType ty = linden::gpl::PlacerConfig::NPLACE;
    linden::gpl::PlacerConfig cfg(ty);
    cfg.target_density = density_;
    cfg.target_overflow = overflow_;
    cfg.use_uniform_density = use_uniform_density_;
    {
      linden::dp::LefDefDumper dumper;
      dumper.run(*ord::OpenRoad::openRoad());
      cfg.input_def = dumper.def();
      cfg.tech_lef = dumper.tech_lef();
      cfg.lib_lefs = dumper.lib_lefs();
    }
    lindengp_ = linden::gpl::Placer::create(linden_db_, cfg, log_);
  }
  std::unique_ptr<Graphics> graphics;
  if (Graphics::guiActive()) {
    graphics = make_unique<Graphics>(log_, linden_db_, pb_);
  }
  const int MAX_ITER = 5000;
  if (gui_debug_) {
    for (size_t iter = 0; iter < MAX_ITER; iter++) {
      auto ret = lindengp_->one_step_move(iter);
      if (graphics) {
        bool update
            = (iter == 0 || (iter + 1) % gui_debug_update_iterations_ == 0);
        bool pause
            = (iter == 0 || (iter + 1) % gui_debug_pause_iterations_ == 0);
        if (update) {
          graphics->cellPlot(pause);
        }
      }
      if (!ret) {
        break;
      }
    }
  } else {
    lindengp_->run(MAX_ITER);
  }

  if (graphics) {
    graphics->status("End placement");
    graphics->cellPlot(true);
  }

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
}


}  // namespace gpl
