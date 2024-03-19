#pragma once

#include <vector>
#include "linden/geom/rect.hpp"

namespace odb {
class dbDatabase;
class dbInst;
class dbNet;
class Rect;
}

namespace linden {

linden::geom::Rect orect_to_lrect(const odb::Rect&) noexcept;
odb::Rect lrect_to_orect(const linden::geom::Rect&) noexcept;

linden::geom::Rect die_region(odb::dbDatabase& odb) noexcept;

// get core region from odb
linden::geom::Rect core_region(odb::dbDatabase& odb) noexcept;

// check if inst/net is supported in linden's workflow
bool is_supported(odb::dbInst& inst) noexcept;
bool is_supported(odb::dbNet& net) noexcept;

// check if inst is fixed (not movable)
bool is_fixed(odb::dbInst& inst) noexcept;

// check if inst is x/y rotated
bool is_inst_x_y_rotated(odb::dbInst& inst) noexcept;

// get the located shape of inst
linden::geom::Rect inst_located_shape(odb::dbInst& inst) noexcept;

// get the rects of inst, especially for macro with OBS.OVERLAP
std::vector<linden::geom::Rect> rects(odb::dbInst& inst) noexcept;

} // namespace linden
