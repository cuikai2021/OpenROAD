#include "ffi.hpp"

#include <string>

#include "linden/eco_placer.hpp"
#include "lrl/CircuitModifier/CircuitModifier.hh"
#include "lrl/Laurel.hh"
#include "odb/geom.h"
#include "spdlog/spdlog.h"

IMPORT_BOOL_FLAG_WITH_DEFAULT(LINDEN_IN_REGRESSION);

namespace linden {

void legalize_instance_gogogo(const std::string& master)
{
    if (LINDEN_IN_REGRESSION()) {
        spdlog::set_pattern("%v"); // no timestamp
    }

    spdlog::debug("Received cell name: {}", master);
    lrl::NetworkModifier& cm = lrl::Laurel::getPlacementNetworkModifier();
    lrl::STANetwork nw = cm.network;

    sta::Instance* sta_inst = nw.findInstance(std::string(master).c_str());
    if (!sta_inst) {
        spdlog::info("Instance not found '{}'.", master);
        return;
    }

    const char* instance = nw.name(nw.cell(sta_inst));
    odb::Point current_point = nw.location(sta_inst);
    spdlog::info("Found instance '{}({})' at ({}, {}).", master, instance, current_point.x(), current_point.y());

    EcoPlacer ep;
    ep.remove_cell(master);
    PlaceIterator pi(ep, instance, current_point);
    auto result = pi.next();

    if (!result) {
        spdlog::info("No available placement position on the chip.");
        return;
    }
    cm.moveInst(sta_inst, result->point, result->orient);

    spdlog::info(
        "Instance '{}({})' placed at ({}, {})\n  dx = {}\n  dy = {}.",
        master,
        instance,
        result->point.x(),
        result->point.y(),
        result->point.x() - current_point.x(),
        result->point.y() - current_point.y());
}

} // namespace linden