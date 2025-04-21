#include "add_tie_cell.hpp"

#include <spdlog/spdlog.h>

#include <boost/process.hpp>
#include <cstdlib>
#include <deque>
#include <string>

#include "../dp/dp.hpp"
#include "db_sta/dbSta.hh"
#include "envflag/envflag.hpp"
#include "fassert.hpp"
#include "linden/dp.hpp"
#include "lrl/CircuitModifier/CircuitModifier.hh"
#include "lrl/Laurel.hh"
#include "odb/db.h"
#include "odb/dbTypes.h"
#include "ord/OpenRoad.hh"
#include "pblefdef.pb.h"
#include "rsz/Resizer.hh"
#include "sta/NetworkClass.hh"

IMPORT_STR_FLAG_WITH_DEFAULT(LINDENDP_EXE_DIR);
IMPORT_STR_FLAG_WITH_DEFAULT(LINDEN_DP_ALGORITHM);

namespace linden {

namespace {

std::filesystem::path get_add_tie_cell_exe()
{
    std::filesystem::path dir(LINDENDP_EXE_DIR());
    return dir / "linden-add-tie-cell";
}

void read_back(const std::filesystem::path& outdef)
{
    std::filesystem::path absolute_path = std::filesystem::absolute(outdef);
    pblefdef::DesignExchangeFormat def;
    {
        std::ifstream fin(outdef);
        if (!fin) {
            spdlog::error("Cannot open result DEF from Linden Add Tie Cell: {}", (std::string) outdef);
            throw std::runtime_error("Cannot open result DEF from Linden Add Tie Cell.");
        }
        bool ret = def.ParseFromIstream(&fin);
        if (!ret) {
            spdlog::error("Cannot parse result DEF from Linden Add Tie Cell: {}", (std::string) outdef);
            throw std::runtime_error("Cannot parse result DEF from Linden Add Tie Cell.");
        }
    }
    ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
    lrl::NetworkModifier& cm = lrl::Laurel::getPlacementNetworkModifier();
    lrl::STANetwork nw = cm.network;
    odb::dbBlock& block = *(ord.getDb()->getChip()->getBlock());

    int64_t pm_per_dbu = 1'000'000 / block.getDefUnits();

    // 1. Add Tie Cell, Pins, Move Cells
    for (size_t i = 0, sz = def.components_size(); i < sz; ++i) {
        const pblefdef::DesignExchangeFormat_Component& comp = def.components(i);
        const std::string& cell_name = comp.name();
        const std::string& model_name = comp.model_name();
        FASSERT(comp.has_place())
        (cell_name);
        const pblefdef::DesignExchangeFormat_Place& place = comp.place();
        FASSERT(place.has_position())
        (cell_name);
        const pblefdef::Point& target_pos = place.position();

        const odb::Point point(target_pos.x() / pm_per_dbu, target_pos.y() / pm_per_dbu);
        const odb::dbOrientType orient(dp::pb_to_dbOrientType(place.orient()));

        sta::Instance* inst = nw.findInstance(cell_name.c_str());
        if (inst == nullptr) {
            auto lib_cell = nw.cell(model_name.c_str());
            inst = cm.createInst(lib_cell, point, cell_name, true);
        }
        cm.moveInst(inst, point, orient.getValue());
    }

    auto getStaPin = [](const ::pblefdef::DesignExchangeFormat_Net_PinRef& pin) -> std::string {
        if (pin.has_cell()) {
            return fmt::format("{}/{}", pin.cell().cell_name(), pin.cell().pin_name());
        } else {
            FASSERT(pin.pin_case() == ::pblefdef::DesignExchangeFormat_Net_PinRef::PinCase::kIo).what("Not Expected Pin Type");
            return pin.io();
        }
    };

    auto net_usage_changed = [&nw](sta::Net* sta_net, const ::pblefdef::DesignExchangeFormat_Net& pb_net) -> bool {
        const auto sta_usage = nw.getDbNet(sta_net)->getSigType();
        const auto pb_usage = pb_net.usage();
        if (sta_usage == odb::dbSigType::GROUND && pb_usage != pblefdef::Use::GROUND) {
            return true;
        }
        if (sta_usage == odb::dbSigType::POWER && pb_usage != pblefdef::Use::POWER) {
            return true;
        }

        return false;
    };

    auto sync_net = [&](sta::Net* sta_net, const ::pblefdef::DesignExchangeFormat_Net& pb_net) {
        std::set<std::string> sta_pins;
        auto cpiter = nw.connectedPinIterator(sta_net);
        while (cpiter->hasNext()) {
            sta_pins.insert(nw.name(cpiter->next()));
        }
        delete cpiter;

        std::set<std::string> pb_pins;
        for (const auto& pin : pb_net.pins()) {
            pb_pins.insert(getStaPin(pin));
        }
        // Del Pin
        for (const auto& pin_name : sta_pins) {
            if (pb_pins.contains(pin_name)) {
                continue;
            }
            cm.disconnPin(nw.findPin(pin_name));
        }

        // Add Pin
        for (const auto& pin_name : pb_pins) {
            if (sta_pins.contains(pin_name)) {
                continue;
            }
            const sta::Pin* sta_pin = nw.findPin(pin_name);
            FASSERT(sta_pin != nullptr)
            (pin_name).what("Cannot found sta pin");
            if (nw.isConnectedHier(sta_pin) || nw.isConnectedFlat(sta_pin)) {
              cm.disconnPin(sta_pin);
            }
            cm.connectPin(sta_pin, sta_net);
        }

        // Mod usage
        auto odb_net = nw.getDbNet(sta_net);
        FASSERT(pb_net.usage() == pblefdef::Use::SIGNAL)(pb_net.name()).what("Add Tie Cell only mod nets usage to signal.");
        odb_net->setSigType(odb::dbSigType::SIGNAL);
    };

    // 2. Add & Mod Nets
    std::unordered_set<std::string> def_nets;
    for (size_t i = 0, sz = def.nets_size(); i < sz; ++i) {
        const ::pblefdef::DesignExchangeFormat_Net& net = def.nets(i);
        const std::string& net_name = net.name();
        sta::Net* sta_net = nw.findNet(net_name.c_str());

        if (sta_net != nullptr) {
            // Mod Nets (Top Level Port)
            if (net_usage_changed(sta_net, net)) {
                sync_net(sta_net, net);
            }
        } else {
            // Add Nets
            sta_net = cm.createNet(net_name, true);
            for (const auto& pin : net.pins()) {
                const std::string pin_name = getStaPin(pin);
                const sta::Pin* sta_pin = nw.findPin(pin_name);
                FASSERT(sta_pin != nullptr)
                (pin_name).what("Cannot found sta pin");
                if (nw.isConnectedHier(sta_pin) || nw.isConnectedFlat(sta_pin)) {
                  cm.disconnPin(sta_pin);
                }
                cm.connectPin(sta_pin, sta_net);
            }
        }
        def_nets.insert(net_name);
    }

    // 3. Delete Nets
    for (auto net : block.getNets()) {
        if (net->isSpecial()) {
            continue;
        }
        const std::string net_name = net->getName();
        if (!def_nets.contains(net_name)) {
            auto sta_net = nw.findNet(net_name.c_str());
            cm.removeNet(sta_net);
        }
    }

    rsz::Resizer& resizer = *ord.getResizer();
    resizer.estimateParasitics(rsz::ParasiticsSrc::placement);
}

} // namespace

int add_tie_cell(const dp::Config& cfg, std::deque<std::string>& args, const std::string& output)
{
    // exec
    int ec = exec(cfg, get_add_tie_cell_exe(), args, LINDEN_DP_ALGORITHM());

    read_back(output);

    return ec;
}

} // namespace linden
