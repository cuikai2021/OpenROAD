#include "rsz/ResizerAddTieCell.hh"
#include "rsz/Resizer.hh"
#include "sta/GraphDelayCalc.hh"
#include "sta/Network.hh"
#include "sta/NetworkClass.hh"
#include "sta/Sdc.hh"
#include "sta/Search.hh"
#include "sta/Liberty.hh"
#include "db_sta/dbNetwork.hh"

#include "fassert.hpp"
#include <string_view>

namespace rsz {

using utl::RSZ;
std::pair<LibertyCell*, LibertyPort*> ResizerAddTieCell::findLibertyCellAndPort(
    const char* cell_name,
    const char* port_name)
{
  {
    auto lib_cell = network_->findLibertyCell(cell_name);
    if (lib_cell == nullptr) {
      logger_->error(RSZ, 204, "cannot find cell {}", cell_name);
    }

    auto lib_port = lib_cell->findLibertyPort(port_name);
    if (lib_port == nullptr) {
      logger_->error(RSZ,
                     206,
                     "cannot find cell pin {} for the cell {}",
                     port_name,
                     cell_name);
    }

    return {lib_cell, lib_port};
  }
}

Instance* ResizerAddTieCell::makeInstance(LibertyCell* cell,
                       const char* name,
                       Instance* parent,
                       const Point& loc,
                       odb::dbSourceType type)
{
  auto inst = Resizer::makeInstance(cell, name, parent, loc);
  auto db_inst = db_network_->staToDb(inst);
  db_inst->setSourceType(type);
  return inst;
}


std::vector<Net*> ResizerAddTieCell::getNameSortedNets()
{
  auto db_network = dynamic_cast<dbNetwork*>(network_);
  odb::dbSet<dbNet> dbNets = db_network->block()->getNets();
  std::vector<Net*> nets;
  nets.reserve(dbNets.size());
  for (auto net : dbNets) {
    nets.push_back(db_network->dbToSta(net));
  }
  std::sort(nets.begin(),
            nets.end(),
            [&db_network](const auto& a, const auto& b) -> bool {
              return strcmp(db_network->name(a), db_network->name(b)) < 0;
            });
  return nets;
};

void
ResizerAddTieCell::addTieCell(const char* tie_lo_cname,
                    const char* tie_lo_pname,
                    const char* tie_hi_cname,
                    const char* tie_hi_pname)
{
  initBlock();
  initDesignArea();

  vector<Net*> gnd_nets, pwr_nets;
  for (auto net : getNameSortedNets()) {
    if (network_->isGround(net)) {
      gnd_nets.push_back(net);
    } else if (network_->isPower(net)) {
      pwr_nets.push_back(net);
    }
  }

  auto is_special_pin = [this](const Pin* pin) {
    FASSERT(pin != nullptr);
    odb::dbITerm *iterm = nullptr;
    odb::dbBTerm *bterm = nullptr;
    odb::dbModITerm* moditerm = nullptr;
    odb::dbModBTerm* modbterm = nullptr;
    db_network_->staToDb(pin, iterm, bterm, moditerm, modbterm);
    return (iterm != nullptr && iterm->isSpecial())
      ||   (bterm != nullptr && bterm->isSpecial());
  };

  auto make_tie_cell = [this, top_inst(network_->topInstance())](LibertyCell* lib_cell, const char* base_name, const Pin* adjacentPin) {
    auto tie_name = makeUniqueInstName(base_name, true);
    // 1. According to the lef/def 5.8 ref, in the COMPONENTS section tie cell source is not DIST,
    //    not TIMING and not USER, only NETLIST can be used.
    // 2. 3rd-party tool does not dump SOURCE type, and by default is NETLIST.
    // 3. A backend expert advises to use NETLIST as SOURCE.
    // Use NONE to dump a tie cell without SOURCE field.
    Point loc = db_network_->location(adjacentPin);
    auto tie_cell = makeInstance(lib_cell, tie_name.c_str(), top_inst, loc, odb::dbSourceType::NONE);

    designAreaIncr(area(network_->cell(lib_cell)));
    return tie_cell;
  };

  auto connect = [this](const Pin* pin, Instance* tie_cell, LibertyPort* lib_port) {
    // 1. There is a contradiction on SOURCE DIST description of the NETS section.
    //    a. > components that only connect to power or ground nets
    //    b. > ... physical components(...), such as ... tie-high and tie-low cells ...
    //    Tie cells are not physical cells.
    // 2. USE TIEOFF is mentioned by the ref.
    // 3. 3rd-party tool does not dump SOURCE and USE(dbSig) type.
    // 4. A backend expert says that without "USE" field, the default is "USE SIGNAL".
    // makeUniqueNet creates a net without SOURCE and with "USE SIGNAL".
    auto net = makeUniqueNet();
    sta_->disconnectPin(const_cast<Pin*>(pin));
    sta_->connectPin(tie_cell, lib_port, net);
    sta_->connectPin(network_->instance(pin), network_->port(pin), net);
    parasiticsInvalid(net);
  };

  auto batch_connect = [this, &make_tie_cell, &connect, &is_special_pin](
    vector<Net*>& nets, const char* tie_cname, const char* tie_pname,
    const char* tie_base_name, const char* tie_log_name
  ) {
    auto [tie_cell, tie_port] = findLibertyCellAndPort(tie_cname, tie_pname);
    size_t new_tie_cell_count = 0;
    for (auto net : nets) {
      std::string_view net_name(network_->name(net));
      auto pin_iter = network_->connectedPinIterator(net);
      const Pin* left_pin = nullptr;
      for (auto pin : staIter2StableDeque(pin_iter)) {
        std::string_view pin_name(network_->name(pin));
        // Intention: some DEFs have net and special net with same name,
        // and OpenDB merges both to one net. So we need skip the pin from special net.
        if (is_special_pin(pin)) {
          // Ensure that non-special net is empty when there is no left pin.
          FASSERT(db_network_->isSpecial(net))
            (net_name.data())
            (pin_name.data());
          continue;
        }
        // Intention: When a pin is a top output port,
        // different net and pin names introduce an extra verilog assigment that connects the port and the net.
        // Use the same name to avoid such assignments.
        if (net_name == pin_name) {
          FASSERT(!db_network_->isSpecial(net))
            (net_name.data())
            (pin_name.data());
          FASSERT(left_pin == nullptr)
            (net_name.data())
            (pin_name.data());
          left_pin = pin;
          continue;
        }
        connect(pin, make_tie_cell(tie_cell, tie_base_name, pin), tie_port);
        new_tie_cell_count += 1;
      }
      delete pin_iter;

      if (left_pin != nullptr) {
        db_network_->staToDb(net)->setSigType(odb::dbSigType::SIGNAL);
        sta_->connectPin(make_tie_cell(tie_cell, tie_base_name, left_pin), tie_port, net);
        new_tie_cell_count += 1;
        parasiticsInvalid(net);
      } else if (db_network_->isSpecial(net)) {
        parasiticsInvalid(net);
      } else {
        sta_->deleteNet(net);
        parasitics_invalid_.erase(net);
      }
    }
    logger_->info(RSZ, 207, "Inserted {} {} cells.", new_tie_cell_count, tie_log_name);
    if (new_tie_cell_count > 0) {
      level_drvr_vertices_valid_ = false;
    }
  };
  batch_connect(gnd_nets, tie_lo_cname, tie_lo_pname, "TIEL", "tie low");
  batch_connect(pwr_nets, tie_hi_cname, tie_hi_pname, "TIEH", "tie high");
}

void
ResizerAddTieCell::removeTieCell(const char* tie_lo_cname,
                       const char* tie_lo_pname,
                       const char* tie_hi_cname,
                       const char* tie_hi_pname)
{
  auto [tie_lo_cell, tie_lo_port] = findLibertyCellAndPort(tie_lo_cname, tie_lo_pname);
  auto [tie_hi_cell, tie_hi_port] = findLibertyCellAndPort(tie_hi_cname, tie_hi_pname);

  size_t removed_tie_cell_count = 0;
  auto inst_iter = network_->childIterator(network_->topInstance());
  while (inst_iter->hasNext()) {
    auto inst = inst_iter->next();

    if (auto lib_cell = network_->libertyCell(inst);
        lib_cell != tie_lo_cell and lib_cell != tie_hi_cell
    ) {
      continue;
    }

    // if (not db_network_->staToDb(inst)->isDoNotTouch()) {
    //   continue;
    // }

    if (sdc_->isConstrained(inst)) {
      continue;
    }

    bool has_constrained_pin = false;
    bool has_constrained_net = false;
    bool has_non_tie_pin_net = false;
    auto pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      auto pin = pin_iter->next();
      if (sdc_->isConstrained(pin)) {
        has_constrained_pin = true;
        break;
      }

      auto net = network_->net(pin);
      if (net == nullptr) {
        continue;
      }

      if (sdc_->isConstrained(net)) {
        has_constrained_net = true;
        break;
      }

      auto lib_port = network_->libertyPort(pin);
      if (lib_port != tie_lo_port and lib_port != tie_hi_port) {
        has_non_tie_pin_net = true;
        break;
      }
    }
    delete pin_iter;
    if (has_constrained_pin or has_constrained_net or has_non_tie_pin_net) {
      continue;
    }

    pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      auto pin = pin_iter->next();
      auto net = network_->net(pin);

      if (net == nullptr) {
        continue;
      }

      auto lib_port = network_->libertyPort(pin);
      if (lib_port == tie_lo_port) {
        db_network_->staToDb(net)->setSigType(odb::dbSigType::GROUND);
      } else {
        FASSERT(lib_port == tie_hi_port);
        db_network_->staToDb(net)->setSigType(odb::dbSigType::POWER);
      }
      sta_->disconnectPin(pin);
      parasiticsInvalid(net);
    }
    sta_->deleteInstance(inst);
    removed_tie_cell_count += 1;
  }
  delete inst_iter;
  logger_->info(RSZ, 216, "Removed {} tie cells.", removed_tie_cell_count);

  if (removed_tie_cell_count > 0) {
    graph_delay_calc_->delaysInvalid();
    search_->arrivalsInvalid();
    level_drvr_vertices_valid_ = false;
  }
}

}