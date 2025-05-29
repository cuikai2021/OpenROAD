#include "ffi.hpp"

#include <string>

#include "linden/eco_placer.hpp"
#include "lrl/CircuitModifier/CircuitModifier.hh"
#include "lrl/Laurel.hh"
#include "odb/geom.h"
#include "spdlog/spdlog.h"

IMPORT_BOOL_FLAG_WITH_DEFAULT(LINDEN_IN_REGRESSION);

namespace linden {

void legalize_instance_gogogo(const std::string& instance)
{
    if (LINDEN_IN_REGRESSION()) {
        spdlog::set_pattern("%v"); // no timestamp
    }

    spdlog::debug("Received cell name: {}", instance);
    lrl::NetworkModifier& cm = lrl::Laurel::getPlacementNetworkModifier();
    lrl::STANetwork nw = cm.network;

    sta::Instance* sta_inst = nw.findInstance(std::string(instance).c_str());
    if (!sta_inst) {
        spdlog::info("Instance not found '{}'.", instance);
        return;
    }

    const char* master = nw.name(nw.cell(sta_inst));
    odb::Point current_point = nw.location(sta_inst);
    spdlog::info("Found instance '{}({})' at ({}, {}).", instance, master, current_point.x(), current_point.y());

    EcoPlacer ep;
    ep.remove_cell(instance);
    PlaceIterator pi(ep, master, current_point);
    auto result = pi.next();

    if (!result) {
        spdlog::info("No available placement position on the chip.");
        return;
    }

    // Create a new instance from the master (libcell) at the given point.
    cm.moveInst(sta_inst, result->point, result->orient);

    spdlog::info(
        "Instance '{}({})' placed at ({}, {})\n  dx = {}\n  dy = {}.",
        instance,
        master,
        result->point.x(),
        result->point.y(),
        result->point.x() - current_point.x(),
        result->point.y() - current_point.y());
}

} // namespace linden