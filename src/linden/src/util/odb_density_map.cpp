#include "linden/odb_density_map.hpp"
#include "odb/db.h"
#include "../util/odb_util.hpp"
#include "fassert.hpp"

using namespace std;

namespace linden {
namespace density {

OdbDensityMap::OdbDensityMap(odb::dbDatabase& odb) noexcept
: DensityMap(core_region(odb), 100, 100) {
    odb::dbBlock* block = odb.getChip()->getBlock();
    auto insts = block->getInsts();
    for(odb::dbInst* inst: insts) {
        if (!is_supported(*inst)) {
            continue;
        }
        auto type = is_fixed(*inst) ? InstType::FIXED : InstType::MOVABLE;
        for (const auto& rect : rects(*inst)) {
            add(rect, 1.0, type);
        }
    }
}

void OdbDensityMap::add_movable_stdcell(odb::dbInst& inst) noexcept {
  FASSERT(!is_fixed(inst));
  add(inst_located_shape(inst), 1.0, InstType::MOVABLE);
}

void OdbDensityMap::remove_movable_stdcell(odb::dbInst& inst) noexcept {
  FASSERT(!is_fixed(inst));
  remove(inst_located_shape(inst), 1.0, InstType::MOVABLE);
}

} // namespace density
} // namespace linden