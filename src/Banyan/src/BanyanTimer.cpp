#include "BanyanTimer.hpp"
#include "lrl/Laurel.hh"
#include "lrl/Timer/Timer.hh"

namespace banyan {

sta::Pin*
BanyanTimer::pin2StaPin(sta::dbNetwork* network, Pin* pin)
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();
  auto instanceName = pin->instancename();
  if (pin->has_portname()) {
    auto inst = block->findInst(instanceName.c_str());
    auto iterm = inst->findITerm(pin->portname().value().c_str());
    return network->dbToSta(iterm);
  } else {
    auto bterm = block->findBTerm(instanceName.c_str());
    return network->dbToSta(bterm);
  }
}

void
BanyanTimer::initWireBudgeting() {;
  sta::dbNetwork* network = ::ord::OpenRoad::openRoad()->getDbNetwork();
  auto sta = ::ord::OpenRoad::openRoad()->getSta();
  auto unitCap = ord::OpenRoad::openRoad()->getResizer()->wireSignalCapacitance(sta->cmdCorner());
  odb::dbTech* tech = db_->getTech();
  auto dbu = tech->getDbUnitsPerMicron();
  logger_->info(utl::BY, 1, "unit cap {}", unitCap);
  if (network->isLinked()) {
    auto chip = db_->getChip();
    auto block = chip->getBlock();

    lrl::GBAReporter gba;
    gba.update();
    for (int i = 0; i < design_->nets_size(); i++) {
      auto netTiming = timingReport->add_nets();
      auto net = design_->mutable_nets(i);
      auto odbNet = block->findNet(net->name().c_str());
      netTiming->set_name(net->name());
      for (int j = 0; j < net->pins_size(); j++) {
        auto pinTiming = netTiming->add_pins();
        auto pin = net->mutable_pins(j);
        auto stapin = pin2StaPin(network, pin);
        pinTiming->set_instancename(pin->instancename());
        if (pin->has_portname()) pinTiming->mutable_portname()->set_value(pin->portname().value());
        pinTiming->mutable_minslack()->set_value(gba.slack(gba.network().vertex(stapin)));
      }
      double wire_cap = laurel_->wireCap(odbNet);
      netTiming->set_capbudget(wire_cap);
      odb::int64 wire_len = (unitCap == 0.0) ? 0 : (odb::int64) (wire_cap / unitCap * METER_TO_MICRON * dbu);
      netTiming->set_wirelenbudget(wire_len);
      logger_->warn(utl::BY,
                    1,
                    "budgeting for net {} cap {} wirelen {}",
                    net->name(),
                    wire_cap,
                    wire_len);
    }
  }
}

}