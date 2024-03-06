#include "linden/odb_util.hpp"

#include "fassert.hpp"
#include "odb/db.h"
#include "odb/dbTransform.h"

using namespace odb;

namespace linden {
namespace odb_util {

linden::geom::Rect core_region(dbDatabase& odb) noexcept
{
    dbBlock* block = odb.getChip()->getBlock();
    Rect coreRect = block->getCoreArea();
    return linden::geom::Rect{
        coreRect.xMin(),
        coreRect.yMin(),
        coreRect.xMax(),
        coreRect.yMax()
    };
}

bool is_supported(dbInst& inst) noexcept {
    auto type = inst.getMaster()->getType();
    return type.isCore() || type.isBlock() || type == dbMasterType::RING || inst.getMaster()->isPad();
}

bool is_supported(dbNet& net) noexcept {
    auto ty = net.getSigType();
    return ty == dbSigType::SIGNAL || ty == dbSigType::CLOCK;
}

bool is_inst_x_y_rotated(dbInst& inst) noexcept
{
    bool x_y_rotated = false;
    switch (inst.getOrient()) {
      case dbOrientType::R0:
      case dbOrientType::R180:
      case dbOrientType::MY:
      case dbOrientType::MX: {
        x_y_rotated = false;
      } break;
      case dbOrientType::R90:
      case dbOrientType::R270:
      case dbOrientType::MYR90:
      case dbOrientType::MXR90: {
        x_y_rotated = true;
      } break;
      default: {
        FASSERT(0 && "unknown orient ");
      } break;
    };
    return x_y_rotated;
}

bool is_fixed(dbInst& inst) noexcept
{
    bool is_fixed = false;
    switch (inst.getPlacementStatus()) {
        case dbPlacementStatus::NONE:
        case dbPlacementStatus::UNPLACED:
        case dbPlacementStatus::SUGGESTED:
        case dbPlacementStatus::PLACED: {
            is_fixed = false;
        } break;
        case dbPlacementStatus::LOCKED:
        case dbPlacementStatus::FIRM:
        case dbPlacementStatus::COVER:{
            is_fixed = true;
        } break;
        default: {
            FASSERT(0 && "unknown placement status");
        } break;
    }
    return is_fixed;
}

linden::geom::Rect inst_located_shape(dbInst& inst) noexcept
{
    int llx, lly;
    inst.getLocation(llx, lly);
    auto master = inst.getMaster();
    assert(master != nullptr);
    int64_t w = master->getWidth();
    int64_t h = master->getHeight();
    if (is_inst_x_y_rotated(inst)) {
        std::swap(w, h);
    }
    int64_t urx = llx + w;
    int64_t ury = lly + h;
    return linden::geom::Rect{llx, lly, urx, ury};
}

std::vector<linden::geom::Rect> rects(dbInst& inst) noexcept {
    std::vector<linden::geom::Rect> obs_overlap_layer;
    auto master = inst.getMaster();
    for (auto obs : master->getObstructions()) {
      auto layer = obs->getTechLayer();
      if (layer->getType() == dbTechLayerType::OVERLAP) {
        Rect rect = obs->getBox();
        dbTransform transform = inst.getTransform();
        transform.apply(rect);
        obs_overlap_layer.push_back(linden::geom::Rect(
            rect.xMin(), rect.yMin(), rect.xMax(), rect.yMax()));
      }
    }
    if (obs_overlap_layer.empty()) {
      auto rect = inst_located_shape(inst);
      obs_overlap_layer.push_back(rect);
    }
    return obs_overlap_layer;
}

} // namespace odb_util
} // namespace linden
