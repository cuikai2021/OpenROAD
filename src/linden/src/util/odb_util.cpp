#include "odb_util.hpp"
#include "ignored_nets.hpp"
#include "linden/geom/bbox.hpp"
#include "linden/geom/rect.hpp"
#include "linden/geom/point.hpp"
#include "linden/geom/density_grids.hpp"
#include "linden/util/name.hpp"
#include "rsz/Resizer.hh"
#include "lrl/CircuitModifier/CircuitModifier.hh"
#include "lrl/Laurel.hh"
#include "sta/StaMain.hh"
#include "sta/Units.hh"
#include "odb/db.h"
#include "odb/dbTransform.h"
#include "ord/OpenRoad.hh"
#include "fassert.hpp"
#include <absl/container/flat_hash_set.h>
#include <deque>

using namespace std;

namespace linden {

linden::geom::Rect orect_to_lrect(const odb::Rect& orect) noexcept {
    return linden::geom::Rect(
        linden::geom::Point(orect.xMin(), orect.yMin()),
        linden::geom::Point(orect.xMax(), orect.yMax())
    );
}

odb::Rect lrect_to_orect(const linden::geom::Rect& lrect) noexcept {
    return odb::Rect(
        lrect.ll().x(),
        lrect.ll().y(),
        lrect.ur().x(),
        lrect.ur().y()
    );
}

linden::geom::Rect core_region(odb::dbDatabase& odb) noexcept
{
    odb::dbBlock* block = odb.getChip()->getBlock();
    return orect_to_lrect(block->getCoreArea());
}

bool is_supported(odb::dbInst& inst) noexcept {
    auto type = inst.getMaster()->getType();
    return type.isCore()
        || type.isBlock();
}

bool is_supported(odb::dbNet& net) noexcept {
    auto ty = net.getSigType();
    return ty == odb::dbSigType::SIGNAL || ty == odb::dbSigType::CLOCK;
}

bool is_inst_x_y_rotated(odb::dbInst& inst) noexcept
{
    switch (inst.getOrient()) {
    case odb::dbOrientType::R0:
    case odb::dbOrientType::R180:
    case odb::dbOrientType::MY:
    case odb::dbOrientType::MX: {
        return false;
    }
    case odb::dbOrientType::R90:
    case odb::dbOrientType::R270:
    case odb::dbOrientType::MYR90:
    case odb::dbOrientType::MXR90: {
        return true;
    }
    default: {
        FASSERT(false)
            ((size_t) inst.getOrient())
            .what("unknown orient");
        return false;
    }
    };
}

bool is_fixed(odb::dbInst& inst) noexcept
{
    switch (inst.getPlacementStatus()) {
    case odb::dbPlacementStatus::NONE:
    case odb::dbPlacementStatus::UNPLACED:
    case odb::dbPlacementStatus::SUGGESTED:
    case odb::dbPlacementStatus::PLACED: {
        return false;
    } break;
    case odb::dbPlacementStatus::LOCKED:
    case odb::dbPlacementStatus::FIRM:
    case odb::dbPlacementStatus::COVER:{
        return true;
    } break;
    default: {
        FASSERT(false)
            ((size_t) inst.getPlacementStatus())
            .what("unknown placement status");
        return false;
    } break;
    }
}

linden::geom::Rect inst_located_shape(odb::dbInst& inst) noexcept
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
    linden::geom::Point ll(llx, lly);
    linden::geom::Point ur(urx, ury);
    if (auto halo = inst.getHalo(); halo != nullptr) {
        ll.x() -= halo->xMin();
        ll.y() -= halo->yMin();
        ur.x() += halo->xMax();
        ur.y() += halo->yMax();
    }
    return linden::geom::Rect(ll, ur);
}

std::vector<linden::geom::Rect> rects(odb::dbInst& inst) noexcept {
    using linden::geom::Rect;
    using linden::geom::Point;

    std::vector<Rect> obs_overlap_layer;
    auto master = inst.getMaster();
    for (auto obs : master->getObstructions()) {
        auto layer = obs->getTechLayer();
        if (layer->getType() == odb::dbTechLayerType::OVERLAP) {
            odb::Rect rect = obs->getBox();
            odb::dbTransform transform = inst.getTransform();
            transform.apply(rect);
            obs_overlap_layer.push_back(Rect(
                Point(rect.xMin(), rect.yMin()),
                Point(rect.xMax(), rect.yMax())));
        }
    }
    if (obs_overlap_layer.empty()) {
        auto rect = inst_located_shape(inst);
        obs_overlap_layer.push_back(rect);
    }
    return obs_overlap_layer;
}

namespace {

linden::geom::Rect die_region(odb::dbDatabase& odb) noexcept
{
    odb::dbBlock* block = odb.getChip()->getBlock();
    return orect_to_lrect(block->getDieArea());
}

void build_cells(
    linden::db::Database& ldb,
    odb::dbDatabase& odb,
    const linden::geom::Rect& core)
{
    odb::dbBlock* block = odb.getChip()->getBlock();
    auto insts = block->getInsts();
    for(odb::dbInst* inst: insts) {
        if (!is_supported(*inst)) {
            continue;
        }

        auto located_shape = inst_located_shape(*inst);
        auto is_macro = inst->getMaster()->getType().isBlock();
        bool is_fixed = linden::is_fixed(*inst);
        auto lcell = linden::db::Cell::create(located_shape);
        lcell->mut_odb_inst() = inst;
        string name(inst->getName());
        lcell->mut_name() = Name(name);
        lcell->mut_macro() = is_macro;
        lcell->mut_fixed() = is_fixed;
        if (is_fixed) {
            // 1. [done here] Let density of fixed insts be 0. So stdcells
            //    are allowed to move inside.
            // 2. For real obs, such as obs on the overlap layer or the instance
            //    bbox if no overlap layer, they are considered as full blockages.
            lcell->mut_density() = 0;
        }
        ldb.add_cell(*lcell);
    }
}

void build_nets(
    linden::db::Database& ldb,
    odb::dbDatabase& odb,
    const IgnoredNets& ignored_nets)
{
    odb::dbBlock* block = odb.getChip()->getBlock();
    for(auto net: block->getNets()) {
        if (!is_supported(*net)) {
            continue;
        }
        auto name = net->getName();
        auto lnet = linden::db::Net::create(net, name);
        lnet->mut_weight() = ignored_nets.get_net_weight(net);
        ldb.add_net(*lnet);
    }
}

void build_pins(
    linden::db::Database& ldb,
    odb::dbDatabase& odb)
{
    odb::dbBlock* block = odb.getChip()->getBlock();
    IsScanChainsIgnoredPin is_sc_ignored_pins(block->getScanChains());
    for(auto net: block->getNets()) {
        if (!is_supported(*net)) {
            continue;
        }
        auto x_net = ldb.odb2net(net);
        if (!x_net) {
            continue;
        }
        auto& l_net = const_cast<linden::db::Net&>(x_net->get());
        for(auto iterm: net->getITerms()) {
            if (is_sc_ignored_pins(iterm)) {
                continue;
            }
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
                linden::geom::BBox<linden::geom::Rect> bbox;
                auto pin_bbox = bbox(pts);
                if (pin_bbox) {
                    linden::geom::Point cell_center(
                        iterm->getInst()->getMaster()->getWidth() / 2,
                        iterm->getInst()->getMaster()->getHeight() / 2);
                    pin_offset = pin_bbox->c() - cell_center;
                }
            }
            auto l_cell = ldb.odb2cell(iterm->getInst());
            if (l_cell) {
                // pins of std cells/macros
                auto term = linden::db::Pin::create_iterm(iterm, name, l_cell->get(), pin_offset);
                ldb.add_pin(*term);
                ldb.connect(l_net, *term);
            } else {
                // external pins
                auto term = linden::db::Pin::create_bterm(iterm, name, pin_offset);
                ldb.add_pin(*term);
                ldb.connect(l_net, *term);
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
                linden::geom::BBox<linden::geom::Rect> bbox;
                auto pin_bbox = bbox(pts);
                if (pin_bbox) {
                    pin_location = pin_bbox->c();
                }
            }
            auto term = linden::db::Pin::create_bterm(bterm, name, pin_location);
            ldb.add_pin(*term);
            ldb.connect(l_net, *term);
        }
    }
}

constexpr int64_t max_boundary() {
    if (numeric_limits<int64_t>::digits <= numeric_limits<size_t>::digits) {
        return numeric_limits<int64_t>::max();
    } else {
        return numeric_limits<size_t>::max();
    }
}

void build_blockages(
    linden::db::Database& ldb,
    odb::dbDatabase& odb,
    const linden::geom::Rect& core)
{
    // turn blockages and fragmented rows into fixed fillers.
    odb::dbSite* smallest_site = nullptr;
    {
        int64_t smallest_site_area = numeric_limits<int64_t>::max();
        for(auto row: odb.getChip()->getBlock()->getRows()) {
            auto site = row->getSite();
            int64_t w = site->getWidth();
            int64_t h = site->getHeight();
            int64_t a = w * h;
            if (a < smallest_site_area) {
                smallest_site_area = a;
                smallest_site = site;
            }
        }
    }
    FASSERT(smallest_site != nullptr)
        .what("There must be rows.");
    linden::geom::DensityGrids grids(
        ldb.core_region(),
        linden::geom::Point(smallest_site->getWidth(), smallest_site->getHeight()));
    spdlog::debug(
        "core region: {}, {}x{}",
        pp::prettyPrint(ldb.core_region()),
        grids.n_rows(),
        grids.n_cols());

    // Uncovers sites where rows are located.
    //
    // Some regions in the core region are not covered by rows.
    // And they should be considered as blockages.
    for(auto row: odb.getChip()->getBlock()->getRows()) {
        auto row_bbox = orect_to_lrect(row->getBBox());
        auto [start_row, start_col, end_row, end_col] = grids.rect_to_indices(row_bbox);
        for(size_t r = start_row; r < end_row; ++r) {
            for(size_t c = start_col; c < end_col; ++c) {
                grids.empty(r, c);
            }
        }
    }

    // Considers large fixed insts as blockages.
    //
    // Fixed physical insts, such as welltaps and endcaps, are ignored
    // in order not to cut partial blockages into pieces.
    absl::flat_hash_set<odb::dbMaster*> ignored;
    absl::flat_hash_set<odb::dbMaster*> not_ignored;
    auto can_ignore = [&](odb::dbInst* inst) -> bool {
        if(!is_supported(*inst)) {
            return true;
        }
        if(!inst->isFixed()) {
            return true;
        }

        auto master = inst->getMaster();
        if (ignored.contains(master)) {
            return true;
        }
        if (not_ignored.contains(master)) {
            return false;
        }
        if (master->isEndCap()) {
            ignored.insert(master);
            return true;
        }
        if (master->isFiller()) {
            // Fillers can be used as welltaps or endcaps sometimes.
            ignored.insert(master);
            return true;
        }
        size_t cnt_nonpg_pins = 0;
        {
            for(auto iterm: inst->getITerms()) {
                switch(iterm->getSigType().getValue()) {
                case odb::dbSigType::POWER: case odb::dbSigType::GROUND: {
                    break;
                }
                default: {
                    ++cnt_nonpg_pins;
                }
                }
            }
        }
        if (cnt_nonpg_pins == 0) {
            int64_t w = master->getWidth();
            int64_t h = master->getHeight();
            auto const& grid_size = grids.grid_size();
            if (w * h <= grid_size.x() * grid_size.y() * 64) {
                // guess that welltaps and endcaps are very small.
                ignored.insert(master);
                return true;
            }
        }
        not_ignored.insert(master);
        return false;
    };
    for(odb::dbInst* inst: odb.getChip()->getBlock()->getInsts()) {
        if (can_ignore(inst)) {
            continue;
        }

        auto master = inst->getMaster();
        int64_t w = master->getWidth();
        int64_t h = master->getHeight();
        auto const& grid_size = grids.grid_size();
        spdlog::debug("remains: {}, {}x{}",
            inst->getName(),
            h / grid_size.y(),
            w / grid_size.x());
        deque<linden::geom::Rect> overlap_obs;
        for (auto obs : master->getObstructions()) {
            auto layer = obs->getTechLayer();
            if (layer->getType() == odb::dbTechLayerType::OVERLAP) {
                odb::Rect rect = obs->getBox();
                odb::dbTransform transform = inst->getTransform();
                transform.apply(rect);
                auto lrect = orect_to_lrect(rect);
                if(auto halo = inst->getHalo(); halo != nullptr) {
                    lrect.ll().x() -= halo->xMin();
                    lrect.ll().y() -= halo->yMin();
                    lrect.ur().x() += halo->xMax();
                    lrect.ur().y() += halo->yMax();
                }
                overlap_obs.push_back(std::move(lrect));
            }
        }
        if (overlap_obs.empty()) {
            auto bbox = inst_located_shape(*inst);
            auto [start_row, start_col, end_row, end_col] = grids.rect_to_indices(bbox);
            for(size_t r = start_row; r < end_row; ++r) {
                for(size_t c = start_col; c < end_col; ++c) {
                    grids.fill(r, c, 1.0);
                }
            }
        } else {
            for(auto const& rect: overlap_obs) {
                auto [start_row, start_col, end_row, end_col] = grids.rect_to_indices(rect);
                for(size_t r = start_row; r < end_row; ++r) {
                    for(size_t c = start_col; c < end_col; ++c) {
                        grids.fill(r, c, 1.0);
                    }
                }
            }
        }
    }

    // shadow sites by blockages
    for(odb::dbBlockage* blockage : odb.getChip()->getBlock()->getBlockages()) {
        auto bbox = orect_to_lrect(blockage->getBBox()->getBox());
        float density = 1.0 - blockage->getMaxDensity() / 100.0;
        if (blockage->isSoft()) {
            // global placement should not touch soft blockages.
            density = 1.0;
        }
        auto [start_row, start_col, end_row, end_col] = grids.rect_to_indices(bbox);
        for(size_t r = start_row; r < end_row; ++r) {
            for(size_t c = start_col; c < end_col; ++c) {
                grids.fill(r, c, density);
            }
        }
    }

    auto rects = grids.to_rects();
    spdlog::debug("collected blockages: {}", rects.size());
    for(auto const& [r, d]: rects) {
        auto dummy = linden::db::Cell::create(r);
        dummy->mut_fixed() = true;
        dummy->mut_density() = d;
        ldb.add_cell(*dummy);
    }
}

odb::dbOrientType from_linden_orient(linden::db::Orient lori) noexcept {
    switch(lori) {
    case linden::db::Orient::N: return odb::dbOrientType::R0;
    case linden::db::Orient::W: return odb::dbOrientType::R90;
    case linden::db::Orient::S: return odb::dbOrientType::R180;
    case linden::db::Orient::E: return odb::dbOrientType::R270;
    case linden::db::Orient::FN: return odb::dbOrientType::MY;
    case linden::db::Orient::FW: return odb::dbOrientType::MYR90;
    case linden::db::Orient::FS: return odb::dbOrientType::MX;
    case linden::db::Orient::FE: return odb::dbOrientType::MXR90;
    default: {
        FASSERT(false)
            ((size_t) lori)
            .what("unknown linden orientation!");
        return odb::dbOrientType::R0;
    }
    }
}

} // namespace

shared_ptr<linden::db::Database> build_linden_db() noexcept {
    ord::OpenRoad::openRoad()->getSta()->ensureClkNetwork();

    odb::dbDatabase& db = *ord::OpenRoad::openRoad()->getDb();
    
    auto core = core_region(db);
    auto die = die_region(db);
    linden::db::Database::Config lcfg = {
        die_region: die,
        core_region: core,
    };

    IgnoredNets ignored_nets;
    auto se_nets = ignored_nets.scan_enable_nets();
    spdlog::debug("[linden/gpl] # of nets connecting scan enables: {}", se_nets.size());
    auto clock_nets = ignored_nets.clock_nets();
    spdlog::debug("[linden/gpl] # of nets connecting clocks: {}", clock_nets.size());
    auto reset_nets = ignored_nets.reset_nets();
    spdlog::debug("[linden/gpl] # of nets connecting resets: {}", reset_nets.size());

    shared_ptr<linden::db::Database> ldb = make_shared<linden::db::Database>(lcfg);
    build_cells(*ldb, db, core);
    build_blockages(*ldb, db, core);
    build_nets(*ldb, db, ignored_nets);
    build_pins(*ldb, db);

    return ldb;
}

void refresh_locations_from_ldb(const linden::db::Database& ldb) noexcept {
    ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
    lrl::NetworkModifier& cm = lrl::Laurel::getPlacementNetworkModifier();
    lrl::STANetwork sta_nw = cm.network;
    for (auto const& x: ldb.moveable_insts()) {
        auto const& lcell = x.get();

        odb::dbInst* ocell = reinterpret_cast<odb::dbInst*>(lcell.odb_inst());
        ocell->setPlacementStatus(odb::dbPlacementStatus::PLACED);
        odb::dbOrientType orient = from_linden_orient(lcell.orient());

        const linden::geom::Rect& r = lcell.rect();
        odb::Point target_pt(r.ll().x(), r.ll().y());
        sta::Instance* sta_inst = sta_nw.findInstance(lcell.name().raw().c_str());
        FASSERT(sta_inst != nullptr)
            .what("Adding or removing cells are so far unsupported.");
        // FIXME: pad awareness on X coordinates
        cm.moveInst(sta_inst, target_pt, orient.getValue());
    }
    rsz::Resizer& resizer = *ord.getResizer();
    resizer.estimateParasitics(rsz::ParasiticsSrc::placement);
}

int64_t hpwl(const linden::db::Database& db) noexcept
{
    int64_t res = 0;
    for(auto const& x: db.nets()) {
        auto const& net = x.get();
        if (net.pins().size() <= 1) {
            continue;
        }
        deque<linden::geom::Point> pts;
        for(auto const& p: net.pins()) {
            auto const& pin = p.get();
            if (auto c = pin.cell(); c) {
                auto const& cell = c->get();
                pts.push_back(cell.rect().c());
            }
        }
        linden::geom::BBox<linden::geom::Rect> bbox;
        auto net_box = bbox(pts);
        FASSERT(net_box);
        res += net_box->dx() + net_box->dy();
    }
    return res;
}

} // namespace linden
