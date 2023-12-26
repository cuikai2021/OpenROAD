#pragma once

#include "linden/density/density_map.hpp"

namespace odb {
class dbDatabase;
class dbInst;
}

namespace linden {
namespace density {

/*
* DensityMap represents the density distribution of a chip's core_region.
* the density 'map' is a vector of bins that cover the chip's core_region
* , where the density value of a bin is: bin.inst_areas / bin.area.
*/
class OdbDensityMap : public DensityMap {
public:

    // build density map from odb
    explicit OdbDensityMap(odb::dbDatabase& odb) noexcept;

    // update density values after adding/removing a movable stdcell
    void add_movable_stdcell(odb::dbInst& inst) noexcept;
    void remove_movable_stdcell(odb::dbInst& inst) noexcept;
};

} // namespace density
} // namespace linden