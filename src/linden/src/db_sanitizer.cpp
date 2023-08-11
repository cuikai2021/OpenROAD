#include "linden/db_sanitizer.hpp"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "utl/Logger.h"
#include <tcl.h>

namespace linden {

LindenDBSanitizer::LindenDBSanitizer() {
  db_ = ::ord::OpenRoad::openRoad()->getDb();
  logger_ = ::ord::OpenRoad::openRoad()->getLogger();
  initLogicalInstSet();
}

LindenDBSanitizer::~LindenDBSanitizer() {}

void LindenDBSanitizer::initLogicalInstSet() {
  auto chip = db_->getChip();
  auto block = chip->getBlock();
  auto nets = block->getNets();
  for (auto net : nets) {
    if (net->isSpecial()) continue;
    auto iterms = net->getITerms();
    for (auto iterm : iterms) {
      logical_insts_.insert(iterm->getInst());
    }
  }
  return ;
}

bool LindenDBSanitizer::isFillerInstByName(odb::dbInst* inst) const {
  std::string name = inst->getMaster()->getName();
  std::transform(name.begin(), name.end(),name.begin(), ::toupper);
  const std::string KEY = "FIL";
  bool contains = (name.find(KEY) != std::string::npos);
  return contains;
}

bool LindenDBSanitizer::isFillerInstByMasterType(odb::dbInst* inst) const {
  return inst->getMaster()->isFiller();
}

bool LindenDBSanitizer::isUnconnected(odb::dbInst* inst) const {
  return logical_insts_.find(inst) == logical_insts_.end();
}

bool LindenDBSanitizer::hasNoLogicPins(odb::dbInst* inst) const {
  //TODO
  //A filler can only have Power and Ground pins
  return true;
}

/*
Quoted from from LEF/DEF 5.8 Reference.
SPACER—Sometimes called a filler cell, this cell is used to fill in space
between regular core cells. The SPACER sub-class needs to be cells with
no logic-pins. Thus even with the sub-class defined, a cell will not be
considered SPACER (also called FILLER) unless it has no logic/signal
pins. A filler can only have Power and Ground pins.
*/
bool LindenDBSanitizer::isFiller(odb::dbInst *inst) const {
  if (inst->isFixed()) {
    // case: ENDCAP, WELLTAP, Pad fillers
    // These cells' master name contains "FIL", but they are fixed after the Floorplan.
    return false;
  }
  return (isFillerInstByName(inst) || isFillerInstByMasterType(inst)) &&
          isUnconnected(inst);
}

size_t LindenDBSanitizer::removeFillers() {
  auto chip = db_->getChip();
  auto block = chip->getBlock();
  auto insts = block->getInsts();
  size_t remove_filler_cnt = 0;
  for (auto inst : insts) {
    if (isFiller(inst)) {
      ++remove_filler_cnt;
      odb::dbInst::destroy(inst);
    }
  }
  return remove_filler_cnt;
}

} // namespace linden ends