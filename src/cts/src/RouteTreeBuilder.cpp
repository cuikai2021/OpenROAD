#include "RouteTreeBuilder.hh"

#include <cassert>
#include <iostream>

#include "odb/db.h"
#include "db_sta/dbNetwork.hh"
#include "Clock.h"
#include "banyan_cts.pb.h"

namespace cts {

void RouteTreeBuilder::setClkInst(ClockInst* ci) {
  data.emplace<0>(ci);
}

void RouteTreeBuilder::setPinName(std::string pin_name) {
  data.emplace<1>(pin_name);
}

void RouteTreeBuilder::setMergePoint(odb::Point point) {
  data.emplace<2>(point);
}

RouteTreeBuilder& RouteTreeBuilder::addSubtree() {
  subtrees.emplace_back();
  return subtrees.back();
}

void RouteTreeBuilder::ClkInst2PinName(sta::dbNetwork* _network, bool is_src) {
  if (std::holds_alternative<ClockInst*>(data)) {
    auto inst = std::get<ClockInst*>(data);
    odb::dbITerm* pin;

    if (inst->isClockBuffer()) {
      if (is_src) {
        pin = inst->getDbInst()->getFirstOutput();
      } else {
        pin = inst->getDbInst()->getFirstInput();
      }
    } else {
      assert(not is_src);
      pin = inst->getDbInputPin();
    }
    setPinName(_network->name(_network->dbToSta(pin)));
  }
  for (auto& subtree : subtrees) {
    subtree.ClkInst2PinName(_network, false);
  }
}

RouteTree* RouteTreeBuilder::toPb() const {
  auto ot = new RouteTree();
  if (std::holds_alternative<std::string>(data)) {
    ot->set_pinname(std::get<std::string>(data));
  } else if (std::holds_alternative<odb::Point>(data)) {
    auto pb_p = ot->mutable_point();
    auto db_p = std::get<odb::Point>(data);
    pb_p->set_x(db_p.getX());
    pb_p->set_y(db_p.getY());
  } else {
    abort();
  }

  for (const auto& subtree : subtrees) {
    ot->mutable_sons()->AddAllocated(subtree.toPb());
  }
  return ot;
}

}  // namespace cts
