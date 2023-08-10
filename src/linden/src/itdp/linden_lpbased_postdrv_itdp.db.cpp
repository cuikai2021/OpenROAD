#include "linden/linden_lpbased_postdrv_itdp.hpp"
#include "linden/db/db.hpp"
#include "linden/geom/rect.hpp"
#include "linden/geom/point.hpp"
#include "linden/geom/bbox.hpp"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "fassert.hpp"

using namespace std;
using namespace odb;

namespace {

linden::geom::Rect core_region(odb::dbDatabase& odb) noexcept
{
    odb::dbBlock* block = odb.getChip()->getBlock();
    odb::Rect coreRect;
    block->getCoreArea(coreRect);
    return linden::geom::Rect{
        coreRect.xMin(),
        coreRect.yMin(),
        coreRect.xMax(),
        coreRect.yMax()
    };
}

bool inst_x_y_rotated(odb::dbInst& inst) noexcept
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

linden::geom::Rect inst_located_shape(odb::dbInst& inst) noexcept
{
    int llx, lly;
    inst.getLocation(llx, lly);

    auto master = inst.getMaster();
    assert(master != nullptr);
    int64_t w = master->getWidth();
    int64_t h = master->getHeight();
    if (inst_x_y_rotated(inst)) {
        std::swap(w, h);
    }
    int64_t urx = llx + w;
    int64_t ury = lly + h;
    return linden::geom::Rect{llx, lly, urx, ury};
}

bool is_fixed(odb::dbInst& inst) noexcept
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

bool is_macro(odb::dbInst* inst, int64_t site_height) {
    FASSERT(inst != nullptr);
    if (inst->getMaster()->getType().isBlock()) {
        return true;
    } else {
        dbBox* bbox = inst->getBBox();
        if (bbox->getDY() > 6 * site_height) {
            return true;
        } 
    }
    return false;
}

linden::geom::Point get_site_size(odb::dbBlock* block) {
    FASSERT(block != nullptr);
    dbSet<dbRow> rows = block->getRows();
    dbRow* firstRow = *(rows.begin());
    linden::geom::Point siteSize(
        firstRow->getSite()->getWidth(),
        firstRow->getSite()->getHeight());
    return siteSize;
}

bool is_supported(odb::dbInst* inst) {
    auto type = inst->getMaster()->getType();
    return type.isCore() || type.isBlock() || type == odb::dbMasterType::RING || inst->getMaster()->isPad();
}

bool is_supported(dbNet* net) {
    auto ty = net->getSigType();
    return ty == odb::dbSigType::SIGNAL || ty == odb::dbSigType::CLOCK;
}

void build_cells(
    linden::db::Database& ldb,
    odb::dbDatabase& odb,
    const linden::geom::Rect& core)
{
    odb::dbBlock* block = odb.getChip()->getBlock();
    auto insts = block->getInsts();
    auto site_size = get_site_size(block);
    for(odb::dbInst* inst: insts) {
        if (!is_supported(inst)) {
            continue;
        }

        auto located_shape = inst_located_shape(*inst);
        auto fixed = is_fixed(*inst);
        if (fixed) {
            auto effective = located_shape.intersect(core);
            if (effective && effective->area() > 0) {
                located_shape = *effective;
            }
        }
        auto name = inst->getName();
        linden::db::Cell lcell(
            inst,
            name,
            located_shape,
            located_shape.area(),
            is_macro(inst, site_size.y()),
            fixed);
        ldb.add_cell(std::move(lcell));
    }
}

void init_nets(
    linden::db::Database& ldb,
    odb::dbDatabase& odb)
{
    odb::dbBlock* block = odb.getChip()->getBlock();
    for(auto net: block->getNets()) {
        if (!is_supported(net)) {
            continue;
        }
        auto name = net->getName();
        linden::db::Net lnet(net, name);
        ldb.add_net(std::move(lnet));
    }
}

void build_pins(
    linden::db::Database& ldb,
    odb::dbDatabase& odb)
{
    odb::dbBlock* block = odb.getChip()->getBlock();
    for(auto net: block->getNets()) {
        if (!is_supported(net)) {
            continue;
        }
        auto l_net = ldb.odb2net(net);
        FASSERT(l_net);
        for(auto iterm: net->getITerms()) {
            std::string name = iterm->getMTerm()->getName();
            linden::geom::Point pin_offset;
            {
                deque<linden::geom::Point> pts;
                for (odb::dbMPin* pin : iterm->getMTerm()->getMPins()) {
                    for (odb::dbBox* box : pin->getGeometry()) {
                        pts.push_back(linden::geom::Point(box->xMin(), box->yMin()));
                        pts.push_back(linden::geom::Point(box->xMax(), box->yMax()));
                    }
                }
                auto pin_bbox = linden::geom::bbox(pts.begin(), pts.end());
                linden::geom::Point cell_center(
                    iterm->getInst()->getMaster()->getWidth() / 2,
                    iterm->getInst()->getMaster()->getHeight() / 2);
                FASSERT(pin_bbox);
                pin_offset = pin_bbox->center() - cell_center;
            }
            auto l_cell = ldb.odb2cell(iterm->getInst());
            if (l_cell) {
                // pins of std cells/macros
                linden::db::ITerm term(iterm, l_net->get(), name, l_cell->get(), pin_offset);
                ldb.add_pin(std::move(term));
            } else {
                // pins of pads
                linden::db::BTerm term(iterm, l_net->get(), name, pin_offset);
                ldb.add_pin(std::move(term));
            }
        }
        for(auto bterm: net->getBTerms()) {
            auto name = bterm->getName();
            linden::geom::Point pin_location;
            {
                deque<linden::geom::Point> pts;
                for (odb::dbBPin* pin : bterm->getBPins()) {
                    auto box = pin->getBBox();
                    pts.push_back(linden::geom::Point(box.xMin(), box.yMin()));
                    pts.push_back(linden::geom::Point(box.xMax(), box.yMax()));
                }
                auto pin_bbox = linden::geom::bbox(pts.begin(), pts.end());
                FASSERT(pin_bbox);
                pin_location = pin_bbox->center();
            }
            linden::db::BTerm term(bterm, l_net->get(), name, pin_location);
            ldb.add_pin(std::move(term));
        }
    }
}

// FIXME: provide util function to construct lindendb from odb
//        and then we can remove all the above util functions.

} // namespace

namespace itdp {

shared_ptr<linden::db::Database> LPBasedPostDrvItdp::create_linden_db() noexcept {
    shared_ptr<linden::db::Database> ldb;
    {
        auto core = core_region(*db_);
        linden::db::Database::Config cfg = {
            core_region : core,
            cells : 0, // number of cells. not really used in linden
            nets : 0, // max number of nets. not really used in linden
            iterm_pins : 0, // max number of iterm_pins. not really used in linden
            bterm_pins : 0, // max number of bterm_pins. not really used in linden
        };

        ldb = std::make_shared<linden::db::Database>(cfg);
        build_cells(*ldb, *db_, core);
        init_nets(*ldb, *db_);
        build_pins(*ldb, *db_);
    }
    return ldb;
}

} // namespace itdp
