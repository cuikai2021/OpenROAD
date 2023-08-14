#include "load_timing_data.hpp"

#include <spdlog/spdlog.h>

namespace linden {
namespace tdp {

TdpConfig::TdpConfig(double _wire_cap,
            double _wire_res,
            const linden::geom::Rect& core_region)
{
    wire_cap = _wire_cap;
    wire_res = _wire_res;
    // FIXME: we should specify individual move_bound on x and y dimension
    cell_move_bound = std::min(core_region.dx(), core_region.dy()) / 2;
    // FIXME: delta_wl_bound = move_bound.x + move_bound.y
    delta_wl_bound = cell_move_bound * 2;
    spdlog::info("cell_move_bound: {}, delta_wl_bound: {}", cell_move_bound, delta_wl_bound);
    wns = true;
    tns = false;
    //fix it
    wire_delay = false;
}

}  // namespace tdp
}  // namespace linden