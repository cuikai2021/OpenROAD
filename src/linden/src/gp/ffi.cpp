#include "ffi.hpp"
#include "initial_place/initial_place.hpp"
#include "../util/ignored_nets.hpp"
#include "../util/odb_util.hpp"
#include "linden/gp.hpp"
#include "linden/db/db.hpp"
#include "linden/geom/bbox.hpp"
#include "linden/gpl/placer.hpp"
#include "linden/gpl/placer_config.hpp"
#include "rsz/Resizer.hh"
#include "lrl/CircuitModifier/CircuitModifier.hh"
#include "lrl/Laurel.hh"
#include "sta/StaMain.hh"
#include "sta/Units.hh"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "utl/metrics.hpp"
#include "fassert.hpp"
#include "envflag/envflag.hpp"
#include <spdlog/spdlog.h>

IMPORT_STR_FLAG(LINDEN_GP_METRICS_FILE);
IMPORT_BOOL_FLAG_WITH_DEFAULT(LINDEN_SPDLOG_DISABLE_TIMESTAMP);

using namespace std;

namespace linden::gp {

namespace {

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
            is_macro,
            is_fixed);
        ldb.add_cell(std::move(lcell));
    }
}

void init_nets(
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

shared_ptr<linden::db::Database> build_db(const Config& cfg) {
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
    spdlog::debug("# of nets connecting scan enables: {}", se_nets.size());
    auto clock_nets = ignored_nets.clock_nets();
    spdlog::debug("# of nets connecting clocks: {}", clock_nets.size());
    auto reset_nets = ignored_nets.reset_nets();
    spdlog::debug("# of nets connecting resets: {}", reset_nets.size());

    shared_ptr<linden::db::Database> ldb = make_shared<linden::db::Database>(lcfg);
    build_cells(*ldb, db, core);
    init_nets(*ldb, db, ignored_nets);
    build_pins(*ldb, db);


    return ldb;
}

void update_to_odb(const linden::db::Database& ldb) noexcept {
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
            .what("Linden DP so far does not add/remove cells. So, everything must be there in ODB & STA.");
        // FIXME: pad awareness on X coordinates
        cm.moveInst(sta_inst, target_pt, north.getValue());
    }
    rsz::Resizer& resizer = *ord.getResizer();
    resizer.estimateParasitics(rsz::ParasiticsSrc::placement);
}

} // namespace

void gp_gogogo(const Config& cfg) {
    if (LINDEN_SPDLOG_DISABLE_TIMESTAMP()) {
        spdlog::set_pattern("%v"); // no timestamp
    }

    shared_ptr<linden::db::Database> ldb = build_db(cfg);

    initial_place(cfg, *ldb);

    unique_ptr<linden::gpl::Placer> gpl;
    {
        linden::gpl::PlacerConfig::PlacerType ty = linden::gpl::PlacerConfig::NPLACE;
        linden::gpl::PlacerConfig ncfg(ty);
        if (cfg.density) {
            ncfg.target_density = *cfg.density;
            ncfg.use_uniform_density = false;
        } else {
            ncfg.target_density = 1.0;
            ncfg.use_uniform_density = true;
        }
        ncfg.target_overflow = cfg.overflow;
        ncfg.cgr_mode = false;
        ncfg.routability_driven = false;
        gpl = linden::gpl::Placer::create(ldb, ncfg);
    }

    const size_t MAX_ITER = 5000;
    size_t iter = 0;
    for (; iter < MAX_ITER; ++iter) {
        auto ret = gpl->one_step_move(iter);
        if (!ret) {
            break;
        }
        auto overflow = *ret;
        if (overflow <= cfg.overflow) {
            break;
        }
    }
    gpl->done();
    gpl.reset();

    metrics::Config mcfg = metrics::Config::sink_to_stderr();
    if (auto opt = LINDEN_GP_METRICS_FILE(); opt) {
        mcfg = metrics::Config::sink_to_file(*opt);
    }
    metrics::Reporter metrics_reporter(mcfg);
    metrics_reporter["gp"]["iter"] = iter + 1;

    update_to_odb(*ldb);
}

} // namespace linden::gp
