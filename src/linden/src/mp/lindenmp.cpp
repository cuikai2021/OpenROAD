#include "linden/macro_place/placer.hpp"
#include "linden/geom/rect.hpp"
#include "linden/geom/point.hpp"
#include "linden/geom/bbox.hpp"
#include "linden/lindenmp.hpp"
#include "gpl/graphics.h"
#include "gpl/placerBase.h"
#include "sta/StaMain.hh"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "utl/Logger.h"
#include "utl/metrics.hpp"
#include "envflag/envflag.hpp"
#include "fassert.hpp"
#include <deque>
#include <iostream>
#include <tcl.h>

using ord::OpenRoad;
using namespace std;

IMPORT_STR_FLAG(LINDEN_MP_METRICS_FILE);

namespace sta {
extern const char *gpl_tcl_inits[];
}

extern "C" {
extern int Gpl_Init(Tcl_Interp* interp);
}

namespace lindenmp {

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
    switch (inst.getOrient()) {
    case odb::dbOrientType::R0:
    case odb::dbOrientType::R180:
    case odb::dbOrientType::MY:
    case odb::dbOrientType::MX: {
        return false;
    }
    default: {
        return true;
    }
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
    if (inst_x_y_rotated(inst)) {
        std::swap(w, h);
    }
    int64_t urx = llx + w;
    int64_t ury = lly + h;

    // TODO:
    // https://z36j6ypbg0.feishu.cn/wiki/wikcnjt6NhO7NdlmIzHZCr67tvm#fN3LzT
    // It is nature to align models of std cells between mGP and cGP.
    // However, the necessarity of cell inflation (as well as left/right padding)
    // is in question. So, we engage no inflation right now.
    return linden::geom::Rect{llx, lly, urx, ury};
}

bool inst_is_fixed(odb::dbInst& inst) noexcept
{
    switch (inst.getPlacementStatus()) {
    case odb::dbPlacementStatus::LOCKED:
    case odb::dbPlacementStatus::FIRM:
    case odb::dbPlacementStatus::COVER: {
        return true;
    }
    default: {
        return false;
    }
    }
}

void build_cells(
    linden::db::Database& ldb,
    odb::dbDatabase& odb,
    const linden::geom::Rect& core)
{
    odb::dbBlock* block = odb.getChip()->getBlock();
    auto insts = block->getInsts();
    for(odb::dbInst* inst: insts) {
        auto type = inst->getMaster()->getType();
        if (!type.isCore() && !type.isBlock() && type != odb::dbMasterType::RING) {
            continue;
        }

        auto located_shape = inst_located_shape(*inst);
        auto is_macro = inst->getMaster()->getType().isBlock();
        auto is_fixed = inst_is_fixed(*inst);
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
    odb::dbDatabase& odb)
{
    odb::dbBlock* block = odb.getChip()->getBlock();
    for(auto net: block->getNets()) {
        if (net->getSigType() != odb::dbSigType::SIGNAL && net->getSigType() != odb::dbSigType::CLOCK) {
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
        if (net->getSigType() != odb::dbSigType::SIGNAL && net->getSigType() != odb::dbSigType::CLOCK) {
            continue;
        }
        auto l_net = ldb.odb2net(net);
        if (!l_net) {
            continue;
        }
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

} // namespace


LindenMacroPlace::LindenMacroPlace(OpenRoad* openroad)
:   db_(nullptr),
    log_(nullptr),
    density_(0),
    overflow_(0),
    use_uniform_density_(false)
{
    Tcl_Interp* tcl_interp = openroad->tclInterp();
    Gpl_Init(tcl_interp);
    sta::evalTclInit(tcl_interp, sta::gpl_tcl_inits);
    db_ = openroad->getDb();
    log_ = openroad->getLogger();
}

int LindenMacroPlace::run()
{
    linden::mpl::Config cfg;
    cfg.density = density_;
    cfg.overflow = overflow_;
    cfg.use_uniform_density = use_uniform_density_;

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

    auto placer = linden::mpl::Placer::create(ldb, std::move(cfg));

    std::unique_ptr<gpl::Graphics> graphics;
    if (gpl::Graphics::guiActive()) {
        graphics = make_unique<gpl::Graphics>(log_, ldb);
    }

    const int MAX_ITER = 5000;
    size_t iter = 0;
    for (; iter < MAX_ITER; iter++) {
        auto ret = placer->one_step_move(iter);
        if (graphics) {
            bool update
                = (iter == 0 || (iter + 1) % debug_update_iters_ == 0);
            bool pause = (iter == 0 || (iter + 1) % debug_pause_iters_ == 0);
            if (update) {
                graphics->cellPlot(pause);
            }
        }
        if (!ret) {
            break;
        }
        auto overflow = *ret;
        if (overflow <= overflow_) {
            break;
        }
    }
    {
        metrics::Config mcfg = metrics::Config::sink_to_stderr();
        if(auto opt = LINDEN_MP_METRICS_FILE(); opt) {
            mcfg = metrics::Config::sink_to_file(*opt);
        }
        metrics::Reporter reporter(mcfg);
        reporter["mpl::iter"] = iter + 1;
    }

    if (graphics) {
        graphics->status("Before legalization of macro-placement");
        graphics->cellPlot(true);
    }
    auto legalized = placer->legalize();
    if (graphics) {
        graphics->status("End macro-placement");
        graphics->cellPlot(true);
    }

    // update db
    for (auto& cell : ldb->cells()) {
        if (cell.odb_instance() == nullptr) {
            // skip placer-added fillers
            continue;
        }
        if (!cell.movable()) {
            continue;
        }
        odb::dbInst* inst = static_cast<odb::dbInst*>(cell.odb_instance());

        const linden::geom::Rect& r = cell.rect();
        inst->setLocation(r.lx(), r.ly());

        auto placement_status = odb::dbPlacementStatus::PLACED;
        if (cell.macro()) {
            placement_status = odb::dbPlacementStatus::FIRM;
        }
        inst->setPlacementStatus(placement_status);
    }

    if (legalized) {
        return 0;
    } else {
        log_->warn(utl::MPL, 1, "fail to do macro-placement legalization.");
        return 1;
    }
}

} // namespace lindenmp
