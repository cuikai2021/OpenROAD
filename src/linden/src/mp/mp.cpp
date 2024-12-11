#include "mp.hpp"
#include "../gp/graphics.hpp"
#include "../util/odb_util.hpp"
#include "linden/macro_place/placer.hpp"
#include "utl/metrics.hpp"
#include "envflag/envflag.hpp"
#include "fassert.hpp"
#include "prettyprint.hpp"
#include <spdlog/spdlog.h>

using namespace std;

IMPORT_STR_FLAG(LINDEN_MP_METRICS_FILE);
IMPORT_BOOL_FLAG_WITH_DEFAULT(LINDEN_IN_REGRESSION);

namespace linden::mp {

namespace {

void firm_macros(const linden::db::Database& ldb) noexcept {
    size_t macro_cnt = 0;
    for (auto& x: ldb.moveable_insts()) {
        auto& cell = x.get();
        if (!cell.macro()) {
            continue;
        }
        auto o_inst = reinterpret_cast<odb::dbInst*>(cell.odb_inst());
        FASSERT(o_inst != nullptr);
        if (o_inst->getPlacementStatus() == odb::dbPlacementStatus::PLACED) {
            o_inst->setPlacementStatus(odb::dbPlacementStatus::FIRM);
            ++macro_cnt;
        }
    }
    spdlog::info("{} macros are fixed.", macro_cnt);
}

} // namespace

int linden_macro_place(const gp::Config& cfg) noexcept {
    if (LINDEN_IN_REGRESSION()) {
        spdlog::set_pattern("%v"); // no timestamp
    }
    spdlog::info("{}", pp::prettyPrint(cfg));

    linden::mpl::Config mcfg;
    {
        if (cfg.density) {
            mcfg.use_uniform_density = false;
            mcfg.density = *cfg.density;
        } else {
            mcfg.use_uniform_density = true;
        }
        mcfg.overflow = cfg.overflow;
    }
    shared_ptr<linden::db::Database> ldb = build_linden_db();
    auto placer = linden::mpl::Placer::create(ldb, std::move(mcfg));

    unique_ptr<gp::Graphics> graphics;
    if (gp::Graphics::enabled()) {
        graphics.reset(new gp::Graphics(ldb));
    }

    const size_t MAX_ITER = 5000;
    size_t iter = 0;
    for(; iter < MAX_ITER; ++iter) {
        auto ret = placer->one_step_move(iter);
        if (!ret) {
            break;
        }
        if (graphics) {
            graphics->one_more_step(iter);
        }
        auto overflow = *ret;
        if (overflow <= cfg.overflow) {
            break;
        }
    }
    auto legalized = placer->legalize();
    placer.reset();
    refresh_locations_from_ldb(*ldb);
    firm_macros(*ldb);
    if (graphics) {
        graphics->finalize();
        graphics.reset();
    }

    {
        metrics::Config mcfg = metrics::Config::sink_to_stderr();
        if(auto opt = LINDEN_MP_METRICS_FILE(); opt) {
            mcfg = metrics::Config::sink_to_file(*opt);
        }
        metrics::Reporter reporter(mcfg);
        reporter["mpl::iter"] = iter + 1;
    }

    if (legalized) {
        return 0;
    } else {
        spdlog::warn("Fail to do macro-placement legalization.");
        return 1;
    }
}

} // namespace linden::mp
