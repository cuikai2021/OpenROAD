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
#include "ord/OpenRoad.hh"
#include "pblefdef.pb.h"
#include "rsz/Resizer.hh"
#include "sta/NetworkClass.hh"

IMPORT_STR_FLAG_WITH_DEFAULT(LINDENDP_EXE_DIR);

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
            throw std::runtime_error(
                "Cannot open result DEF from Linden Add Tie Cell.");
        }
        bool ret = def.ParseFromIstream(&fin);
        if (!ret) {
            spdlog::error("Cannot parse result DEF from Linden Add Tie Cell: {}", (std::string) outdef);
            throw std::runtime_error(
                "Cannot parse result DEF from Linden Add Tie Cell.");
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
        const odb::dbOrientType orient(pb_to_dbOrientType(place.orient()));

        sta::Instance* inst = nw.findInstance(cell_name.c_str());
        if (inst == nullptr) {
            auto lib_cell = nw.cell(model_name.c_str());
            inst = cm.createInst(lib_cell, point, cell_name, true);
        }
        cm.moveInst(inst, point, orient.getValue());
    }

    // 2. Add Tie Cell Nets
    std::unordered_set<std::string> def_nets;
    for (size_t i = 0, sz = def.nets_size(); i < sz; ++i) {
        const ::pblefdef::DesignExchangeFormat_Net& net = def.nets(i);
        const std::string& net_name = net.name();
        def_nets.insert(net_name);

        sta::Net* sta_net = nw.findNet(net_name.c_str());
        if (sta_net != nullptr) {
            continue;
        }
        sta_net = cm.createNet(net_name, true);

        auto getStaPin = [](const ::pblefdef::DesignExchangeFormat_Net_PinRef& pin)
            -> std::string {
            if (pin.has_cell()) {
                std::stringstream ss;
                ss << pin.cell().cell_name() << "/" << pin.cell().pin_name();
                return ss.str();
            } else {
                FASSERT(pin.pin_case() == ::pblefdef::DesignExchangeFormat_Net_PinRef::PinCase::kIo)
                ("Not Expected Pin Type");
                return pin.io();
            }
        };

        for (const auto& pin : net.pins()) {
            const std::string pin_name = getStaPin(pin);
            const sta::Pin* sta_pin = nw.findPin(pin_name);
            FASSERT(sta_pin != nullptr)
            ("Cannot found sta pin")(pin_name);
            cm.connectPin(sta_pin, sta_net);
        }
    }

    // 3. Delete Ground/Power Nets
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

int add_tie_cell(const DpConfig& cfg, std::deque<std::string>& args, const std::string& output)
{
    // exec
    int ec = exec(cfg, get_add_tie_cell_exe(), args);

    read_back(output);

    return ec;
}

} // namespace linden
