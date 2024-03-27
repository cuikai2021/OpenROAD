#include "linden/gp.hpp"
#include "graphics.hpp"
#include "initial_place/initial_place.hpp"
#include "../util/ignored_nets.hpp"
#include "../util/odb_util.hpp"
#include "../util/relevance_verify.hpp"
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
#include <limits>
#include <tuple>
#include <cmath>
#include <optional>

IMPORT_STR_FLAG(LINDEN_GP_METRICS_FILE);
IMPORT_BOOL_FLAG_WITH_DEFAULT(LINDEN_SPDLOG_DISABLE_TIMESTAMP);
IMPORT_BOOL_FLAG_WITH_DEFAULT(LINDEN_GP_CELL_RELATIVE_POS_CAL);

using namespace std;

namespace linden::gp {

const float DEFAULT_DENSITY = 1.0;
const float DEFAULT_OVERFLOW = 0.1;
const bool DEFAUTL_TIMING_DRIVEN = false;

Config::Config()
:   density(DEFAULT_DENSITY),
    overflow(DEFAULT_OVERFLOW),
    timing_driven(DEFAUTL_TIMING_DRIVEN)
{}

shared_ptr<Config> Config::get_default() noexcept {
    static shared_ptr<Config> cfg(new Config());
    return cfg;
}

void Config::prettyPrint(std::string& out) const noexcept {
    out.append(string_view("linden::gp::Config {"));

    if (density) {
        out.append(string_view("\n  density: "));
        pp::prettyPrint(out, *density);
    } else {
        out.append(string_view("\n  uniform-density"));
    }

    out.append(string_view("\n  overflow: "));
    pp::prettyPrint(out, overflow);

    out.append(string_view("\n  initial-place-policy: "));
    pp::prettyPrint(out, initial_place_policy);

    if (timing_driven) {
        out.append(string_view("\n  timing-driven"));
    }

    out.append(string_view("\n}"));
}

OptionalSetter<float> Config::mut_density() noexcept {
    float d = density.value_or(DEFAULT_DENSITY);
    return OptionalSetter<float>(density, d);
}

namespace {

const char CSTR_DEF[] = "def";
const char CSTR_DIRICHLET[] = "dirichlet";
const char CSTR_EIGEN_VEC[] = "ev";

} // namespace

string_view Config::InitialPlacePolicy::to_str_view() const noexcept {
    switch (value) {
    case DEF:
        return string_view(CSTR_DEF, sizeof(CSTR_DEF) / sizeof(CSTR_DEF[0]) - 1);
    case DIRICHLET:
        return string_view(CSTR_DIRICHLET, sizeof(CSTR_DIRICHLET) / sizeof(CSTR_DIRICHLET[0]) - 1);
    case EIGEN_VEC:
        return string_view(CSTR_EIGEN_VEC, sizeof(CSTR_EIGEN_VEC) / sizeof(CSTR_EIGEN_VEC[0]) - 1);
    }
    FASSERT(false);
    return string_view();
}

void Config::InitialPlacePolicy::prettyPrint(std::string& out) const noexcept {
    out.append(to_str_view());
}

Config::InitialPlacePolicy Config::InitialPlacePolicy::from_str(const string_view& v) noexcept
{
    const string_view SV_DEF(
        CSTR_DEF, sizeof(CSTR_DEF) / sizeof(CSTR_DEF[0]) - 1);
    const string_view SV_DIRICHLET(
        CSTR_DIRICHLET, sizeof(CSTR_DIRICHLET) / sizeof(CSTR_DIRICHLET[0]) - 1);
    const string_view SV_EV(
        CSTR_EIGEN_VEC, sizeof(CSTR_EIGEN_VEC) / sizeof(CSTR_EIGEN_VEC[0]) - 1);
    if (v == SV_DEF) {
        return Config::InitialPlacePolicy(DEF);
    } else if (v == SV_DIRICHLET) {
        return Config::InitialPlacePolicy(DIRICHLET);
    } else if (v == SV_EV) {
        return Config::InitialPlacePolicy(EIGEN_VEC);
    } else {
        FASSERT(false)
            (v)
            .what("Unknown initial-place policy.");
        return Config::InitialPlacePolicy(DEF);
    }
}

namespace {

bool update_net_weights(
    const Config& cfg,
    linden::db::Database& ldb)
{
    refresh_locations_from_ldb(ldb);
    rsz::Resizer* rs = ::ord::OpenRoad::openRoad()->getResizer();
    rs->findResizeSlacks();

    // get Top 10% worst resize nets
    sta::NetSeq& worst_slack_nets = rs->resizeWorstSlackNets();
    if (worst_slack_nets.empty()) {
        spdlog::warn("[linden/gpl] No net slacks found. Timing-driven mode disabled.");
        return false;
    }

    // min/max slack for worst 10% nets
    sta::Slack slack_min = 0;
    {
        auto s = rs->resizeNetSlack(worst_slack_nets.front());
        FASSERT(s)
            .what("Worst slack net has no slack.");
        slack_min = *s;
    }
    sta::Slack slack_max = 0;
    {
        auto s = rs->resizeNetSlack(worst_slack_nets.back());
        FASSERT(s)
            .what("Worst slack net has no slack.");
        slack_max = *s;
    }

    spdlog::info("[linden/gpl] worst 10%% slacks {:.3g}, {:.3g}", slack_min, slack_max);

    if (isinf(slack_min)) {
        spdlog::warn("[linden/gpl] No net slacks found. Timing-driven mode disabled.");
        return false;
    }

    size_t reweight_net_cnt = 0;
    const double NET_WEIGHT_SCALE = 0.9;
    for(auto& l_net: ldb.mut_nets()) {
        odb::dbNet* o_net = reinterpret_cast<odb::dbNet*>(l_net.odb_net());
        double weight = 1.0;
        if (l_net.pins().size() > 1) {
            if (auto s = rs->resizeNetSlack(o_net); s) {
                if (s < slack_max) {
                    weight = 1 + NET_WEIGHT_SCALE * (slack_max - *s) / (slack_max - slack_min);
                    ++reweight_net_cnt;
                    spdlog::debug(
                        "[linden/gpl] timing: net:{} slack:{} weight:{}",
                        l_net.name(),
                        *s,
                        weight);
                }
            }
        }
        l_net.mut_weight() = weight;
    }

    spdlog::info("[linden/gpl] Reweighted {} nets.", reweight_net_cnt);
    return true;
}

} // namespace

void linden_global_place(const Config& cfg) noexcept {
    if (LINDEN_SPDLOG_DISABLE_TIMESTAMP()) {
        spdlog::set_pattern("%v"); // no timestamp
    }
    spdlog::info("{}", pp::prettyPrint(cfg));

    shared_ptr<linden::db::Database> ldb = build_linden_db();

    unique_ptr<Graphics> graphics;
    if (Graphics::enabled()) {
        graphics.reset(new Graphics(ldb));
    }

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

    deque<float> timing_overflows;
    if (cfg.timing_driven) {
        spdlog::info("[linden/gpl] enable timing-driven");
        timing_overflows = {79, 64, 49, 29, 21, 15};
        rsz::Resizer* rs = ::ord::OpenRoad::openRoad()->getResizer();
        rs->resizeSlackPreamble();
    }

    optional<RelevanceDetector> relev;
    if (LINDEN_GP_CELL_RELATIVE_POS_CAL()) {
        relev = std::move(RelevanceDetector(*ldb));
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

        if (relev && iter % 10 == 0) {
            auto const& [x_change, y_change] = relev->update(*ldb);
            spdlog::info(
                "[nPlace] relative position change = ({:.2f}%, {:.2f}%)",
                x_change * 100, y_change * 100);
        }
        if (graphics) {
            graphics->one_more_step(iter);
        }
        if (!timing_overflows.empty()
            && overflow * 100 < timing_overflows.front()) {
            if (!update_net_weights(cfg, *ldb)) {
                spdlog::info("[linden/gpl] Disable timing-driven because of some faults.");
                timing_overflows.clear();
            } else {
                timing_overflows.pop_front();
            }
        }
    }
    gpl->done();
    gpl.reset();
    refresh_locations_from_ldb(*ldb);
    if (graphics) {
        graphics->finalize();
        graphics.reset();
    }

    metrics::Config mcfg = metrics::Config::sink_to_stderr();
    if (auto opt = LINDEN_GP_METRICS_FILE(); opt) {
        mcfg = metrics::Config::sink_to_file(*opt);
    }
    metrics::Reporter metrics_reporter(mcfg);
    metrics_reporter["gp"]["iter"] = iter + 1;
}

} // namespace linden::gp
