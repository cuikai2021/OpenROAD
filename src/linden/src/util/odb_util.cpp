#include "odb_util.hpp"
#include "ignored_nets.hpp"
#include "linden/geom/bbox.hpp"
#include "rsz/Resizer.hh"
#include "lrl/CircuitModifier/CircuitModifier.hh"
#include "lrl/Laurel.hh"
#include "sta/StaMain.hh"
#include "sta/Units.hh"
#include "odb/db.h"
#include "odb/dbTransform.h"
#include "ord/OpenRoad.hh"
#include "fassert.hpp"
#include <deque>

using namespace std;

namespace linden {

linden::geom::Rect orect_to_lrect(const odb::Rect& orect) noexcept {
    return linden::geom::Rect(
        orect.xMin(),
        orect.yMin(),
        orect.xMax(),
        orect.yMax()
    );
}

odb::Rect lrect_to_orect(const linden::geom::Rect& lrect) noexcept {
    return odb::Rect(
        lrect.lx(),
        lrect.ly(),
        lrect.ux(),
        lrect.uy()
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
        || type.isBlock()
        || type == odb::dbMasterType::RING
        || inst.getMaster()->isPad();
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
    return linden::geom::Rect{llx, lly, urx, ury};
}

std::vector<linden::geom::Rect> rects(odb::dbInst& inst) noexcept {
    std::vector<linden::geom::Rect> obs_overlap_layer;
    auto master = inst.getMaster();
    for (auto obs : master->getObstructions()) {
        auto layer = obs->getTechLayer();
        if (layer->getType() == odb::dbTechLayerType::OVERLAP) {
            odb::Rect rect = obs->getBox();
            odb::dbTransform transform = inst.getTransform();
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
        if (is_fixed) {
            // FIXME: to support obs layer
        }
        auto name = inst->getName();
        linden::db::Cell lcell(
            inst,
            name,
            located_shape,
            located_shape.area(),
            is_macro,
            is_fixed);
        ldb.add_cell(std::move(lcell));
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
        linden::db::Net lnet(net, name);
        lnet.mut_weight() = ignored_nets.get_net_weight(net);
        ldb.add_net(std::move(lnet));
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
        auto l_net = ldb.odb2net(net);
        if (!l_net) {
            continue;
        }
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
                auto pin_bbox = linden::geom::bbox(pts.begin(), pts.end());
                linden::geom::Point cell_center(
                    iterm->getInst()->getMaster()->getWidth() / 2,
                    iterm->getInst()->getMaster()->getHeight() / 2);
                if (pin_bbox) {
                    pin_offset = pin_bbox->center() - cell_center;
                }
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
                if (pin_bbox) {
                    pin_location = pin_bbox->center();
                }
            }
            linden::db::BTerm term(bterm, l_net->get(), name, pin_location);
            ldb.add_pin(std::move(term));
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

tuple<size_t, size_t, size_t, size_t> bbox_to_site_range(
    const linden::geom::Rect& core,
    const odb::Rect& bbox,
    int64_t site_width,
    int64_t site_height)
{
    static constexpr int64_t MAX_BOUNDARY = max_boundary();
    linden::geom::Rect l_bbox = orect_to_lrect(bbox);
    if (auto overlap = core.intersect(l_bbox); overlap) {
        if (overlap->area() > 0) {
            int64_t llr = (overlap->ly() - core.ly() + 1) / site_height;
            FASSERT(llr >= 0 && llr <= MAX_BOUNDARY)(llr);
            int64_t llc = (overlap->lx() - core.lx() + 1) / site_width;
            FASSERT(llc >= 0 && llc <= MAX_BOUNDARY)(llc);
            int64_t urr = (overlap->uy() - core.ly() - 1) / site_height;
            FASSERT(urr >= 0 && urr <= MAX_BOUNDARY)(urr);
            int64_t urc = (overlap->ux() - core.lx() - 1) / site_width;
            FASSERT(urc >= 0 && urc <= MAX_BOUNDARY)(urc);
            return make_tuple(llr, llc, urr, urc);
        }
    }
    // just an empty range
    return make_tuple(1, 1, 0, 0);
}

void build_blockages(
    linden::db::Database& ldb,
    odb::dbDatabase& odb,
    const linden::geom::Rect& core)
{
    // turn blockages and fragmented rows into fixed fillers.
    odb::dbSite* smallest_site = nullptr;
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
    FASSERT(smallest_site != nullptr)
        .what("There must be rows.");
    int64_t site_width = smallest_site->getWidth();
    FASSERT(site_width > 1)
        (site_width)
        .what("Width of site is too small.");
    int64_t site_height = smallest_site->getHeight();
    FASSERT(site_height)
        (site_height)
        .what("Height of site is too small.");
    size_t n_rows = core.dy() / site_height;
    size_t n_cols = core.dx() / site_width;

    deque<deque<float>> arena_emptiness;
    // initialize and shadow all sites
    for(size_t i = 0; i < n_rows; ++i) {
        arena_emptiness.emplace_back(n_cols, 0);
    }
    // uncover sites where rows cover
    for(auto row: odb.getChip()->getBlock()->getRows()) {
        auto row_bbox = row->getBBox();
        size_t start_row, start_col, end_row, end_col;
        tie(start_row, start_col, end_row, end_col) = bbox_to_site_range(core, row_bbox, site_width, site_height);
        for(size_t i = start_row; i <= end_row; ++i) {
            for(size_t j = start_col; j <= end_col; ++j) {
                arena_emptiness[i][j] = 100;
            }
        }
    }
    // uncover sites by fixed cells (which may be not on rows)
    // it is weird to uncover these sites and then shadow them again
    // (in another function). But, so be it.
    for(odb::dbInst* inst: odb.getChip()->getBlock()->getInsts()) {
        if(!inst->isFixed()) {
            continue;
        }
        // FIXME: to support obs layer
        auto bbox = inst->getBBox()->getBox();
        size_t start_row, start_col, end_row, end_col;
        tie(start_row, start_col, end_row, end_col) = bbox_to_site_range(core, bbox, site_width, site_height);
        for(size_t i = start_row; i <= end_row; ++i) {
            for(size_t j = start_col; j <= end_col; ++j) {
                arena_emptiness[i][j] = 100;
            }
        }
    }
    // shadow sites by blockages
    for(odb::dbBlockage* blockage : odb.getChip()->getBlock()->getBlockages()) {
        auto bbox = blockage->getBBox()->getBox();
        float blk_emptiness = blockage->getMaxDensity();
        if(blockage->isSoft()) {
            // global placement should not touch soft blockages.
            blk_emptiness = 0;
        }
        size_t start_row, start_col, end_row, end_col;
        tie(start_row, start_col, end_row, end_col) = bbox_to_site_range(core, bbox, site_width, site_height);
        for(size_t i = start_row; i <= end_row; ++i) {
            for(size_t j = start_col; j <= end_col; ++j) {
                arena_emptiness[i][j] = min(arena_emptiness[i][j], blk_emptiness);
            }
        }
    }
    // turn sites to fixed fillers
    auto add_filler_if_neccessary = [&](
        size_t r,
        size_t start_c,
        size_t end_c,
        float emptiness)
    {
        if(emptiness >= 100) {
            return;
        }
        int64_t lly = core.ly() + r * site_height;
        int64_t ury = lly + site_height;
        int64_t llx = core.lx() + start_c * site_width;
        int64_t urx = core.lx() + end_c * site_width;
        linden::geom::Rect bbox(llx, lly, urx, ury);
        linden::db::Cell dummy(bbox, true, (100.0 - emptiness) / 100.0);
        ldb.add_cell(std::move(dummy));
    };
    for(size_t r = 0; r < n_rows; ++r) {
        auto const& row = arena_emptiness[r];
        size_t last_col = 0;
        for(; last_col < n_cols; ) {
            size_t c = last_col + 1;
            float last_emptiness = row[last_col];
            for(; c < n_cols && row[c] == last_emptiness; ++c);
            add_filler_if_neccessary(r, last_col, c, last_emptiness);
            last_col = c;
        }
    }
}

} // namespace

shared_ptr<linden::db::Database> build_linden_db() noexcept {
    odb::dbDatabase& db = *ord::OpenRoad::openRoad()->getDb();
    auto core = core_region(db);
    auto die = die_region(db);
    linden::db::Database::Config lcfg = {
        die_region: die,
        core_region: core,
        cells: 0, // number of cells. not really used in linden
        nets: 0, // max number of nets. not really used in linden
        iterm_pins: 0, // max number of iterm_pins. not really used in linden
        bterm_pins: 0, // max number of bterm_pins. not really used in linden
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
    odb::dbOrientType north = odb::dbOrientType::R0;
    for (auto const& lcell: ldb.cells()) {
        if (!lcell.is_instance()) {
            continue;
        }
        if (lcell.is_virtual()) {
            continue;
        }
        if (!lcell.movable()) {
            continue;
        }

        odb::dbInst* ocell = reinterpret_cast<odb::dbInst*>(lcell.odb_instance());
        ocell->setPlacementStatus(odb::dbPlacementStatus::PLACED);

        const linden::geom::Rect& r = lcell.rect();
        odb::Point target_pt(r.lx(), r.ly());
        sta::Instance* sta_inst = sta_nw.findInstance(lcell.name().c_str());
        FASSERT(sta_inst != nullptr)
            .what("Adding or removing cells are so far unsupported.");
        // FIXME: pad awareness on X coordinates
        cm.moveInst(sta_inst, target_pt, north.getValue());
    }
    rsz::Resizer& resizer = *ord.getResizer();
    resizer.estimateParasitics(rsz::ParasiticsSrc::placement);
}

} // namespace linden
