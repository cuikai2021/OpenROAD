#include "linden/gp.hpp"

#include "gp/netweighter.hpp"
#include "gp/hpwl_analyze.hpp"
#include "graphics.hpp"
#include "initial_place/initial_place.hpp"
#include "../util/odb_util.hpp"
#include "../util/relevance_verify.hpp"
#include "linden/db/db.hpp"
#include "linden/geom/bbox.hpp"
#include "linden/gpl/placer.hpp"
#include "linden/gpl/placer_config.hpp"
#include "linden/rdp/congestion_estimation.hpp"
#include "banyan/Banyan.h"
#include "sc/ScanChain.h"
#include "sc/ScanChainEditer.h"
#include "rsz/Resizer.hh"
#include "lrl/CircuitModifier/CircuitModifier.hh"
#include "ord/OpenRoad.hh"
#include "utl/metrics.hpp"
#include "banyan_cmap_v2.pb.h"
#include "fassert.hpp"
#include "envflag/envflag.hpp"

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <random>
#include <utility>

IMPORT_STR_FLAG(LINDEN_GP_METRICS_FILE);
IMPORT_BOOL_FLAG_WITH_DEFAULT(LINDEN_IN_REGRESSION);
IMPORT_BOOL_FLAG_WITH_DEFAULT(LINDEN_GP_CELL_RELATIVE_POS_CAL);
IMPORT_I64_FLAG_WITH_DEFAULT(LINDEN_GP_CMAP_PERIOD);
IMPORT_I64_FLAG_WITH_DEFAULT(LINDEN_GP_CMAP_ITERATION);
IMPORT_STR_FLAG_WITH_DEFAULT(LINDEN_TMP_FILE_ROOT_DIR);

using namespace std;

namespace linden::gp {

const float DEFAULT_DENSITY = 1.0;

Config::Config()
:   overflow(0.1),
    timing_driven(false),
    continue_mode(false),
    routability_driven(false),
    overflow_to_estimate_cmap(0.2)
{}

shared_ptr<Config> Config::get_default() noexcept {
    static shared_ptr<Config> cfg(new Config());
    return cfg;
}

void Config::prettyPrint(string& out) const noexcept {
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

    if (continuation) {
        out.append(string_view("\n  continuation: "));
        out.append(continuation->string());
    }

    if (continue_mode) {
        out.append(string_view("\n  enable continue-mode"));
    }

    if (highest_layer_to_route) {
        out.append(string_view("\n  max-layer: "));
        out.append(*highest_layer_to_route);
    }

    if (ldb_path) {
        out.append(string_view("\n  ldb-path: "));
        out.append(ldb_path->string());
    }

    out.append(string_view("\n}"));
}

OptionalSetter<float> Config::mut_density() noexcept {
    float d = density.value_or(DEFAULT_DENSITY);
    return OptionalSetter<float>(density, d);
}

OptionalSetter<string> Config::mut_highest_layer_to_route() noexcept {
    return OptionalSetter<string>(highest_layer_to_route, string());
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

void Config::InitialPlacePolicy::prettyPrint(string& out) const noexcept {
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

class ScanChainEditerLdnDBCallBack : public sc::sc_editer::ScanChainEditerCallBackBase
{
public:
    explicit ScanChainEditerLdnDBCallBack(linden::db::Database& ldb) : ldb_(ldb) {}
    void disconnectAfter(odb::dbITerm* p, odb::dbNet* n) override
    {
        // Database doesn't supply odb2pin func, fetch it from instance.
        auto ldb_pin = getLDBPin(p);
        if (ldb_pin) {
            ldb_.disconnect(ldb_pin->get());
        }
    }
    void connectAfter(odb::dbITerm* p, odb::dbNet* n) override
    {
        auto ldb_pin = getLDBPin(p);
        auto ldb_net = ldb_.odb2net(n);
        if (ldb_pin) {
            ldb_.connect(ldb_net->get(), ldb_pin->get());
        }
    }
    void createNetAfter(odb::dbNet* n) override
    {
        auto lnet = linden::db::Net::create(n, n->getName());
        ldb_.add_net(*lnet);
    }
    void removeNetBefore(odb::dbNet* n) override
    {
        auto lnet = ldb_.odb2net(n);
        if (!lnet) {
            return;
        }
        ldb_.remove_net(lnet->get());
    }
private:
    std::optional<db::PinRef> getLDBPin(odb::dbITerm* iterm)
    {
        auto db_inst = iterm->getInst();
        auto ldb_inst = ldb_.odb2cell(db_inst);
        if (!ldb_inst) {
            return std::nullopt;
        }
        auto pin_vec = ldb_inst->get().pins();
        for (auto ldb_pin : pin_vec) {
            if (ldb_pin.get().odb_term() == ((void*) iterm)) {
                return ldb_pin;
            }
        }
        return nullopt;
    }
    linden::db::Database& ldb_;
};

void scanchain_reorder(linden::db::Database& ldb) {
    lrl::Laurel::registerSCEditer();
    ScanChainEditerLdnDBCallBack linden_db_sc_reorder_callback(ldb);
    sc::sc_editer::registerCallBack(&linden_db_sc_reorder_callback);
    auto sc = ::ord::OpenRoad::openRoad()->getScanChain();
    sc->reorder();
    sc::sc_editer::unregisterCallBack(&linden_db_sc_reorder_callback);
}

std::pair<filesystem::path, filesystem::path> get_tmp_path(std::size_t iter) {
    filesystem::path dir(LINDEN_TMP_FILE_ROOT_DIR());
    filesystem::path cmap_file;
    filesystem::path ldb_file;

    cmap_file = dir / fmt::format("banyan_{}.cmap", iter);
    ldb_file = dir / fmt::format("ldb_{}.pb", iter);
    
    return std::make_pair(cmap_file, ldb_file);
}

void estimate_cmap(
    filesystem::path cmap_file,
    const gp::Config& cfg,
    size_t mm_iter,
    size_t gcell_size)
{
    FASSERT(cfg.highest_layer_to_route);
    auto& banyan = *::ord::OpenRoad::openRoad()->getBanyan();
    spdlog::info("[linden/gpl] Dump a banyan cmap file: {}", cmap_file);
    banyan.repair_congestion(
        cmap_file.string(),
        cmap_file.string() + ".cgr.db",
        *cfg.highest_layer_to_route,
        true,
        std::nullopt,
        std::nullopt);
}

double refresh_cmap_to_placer(
    linden::gpl::Placer& placer,
    const filesystem::path& cmap_file)
{
    linden::rdp::CongestionMap cmap;
    banyan::CongestionMap banyan_cmap;
    ifstream fin(cmap_file, ios::binary);
    if (!fin) {
        spdlog::error("Fail to open file: {}", cmap_file);
        throw runtime_error("fail to open file");
    }
    if (!banyan_cmap.ParseFromIstream(&fin)) {
        spdlog::error("Fail to parse banyan cmap file: {}", cmap_file);
        throw runtime_error("fail to parse banyan cmap file");
    }
    
    int64_t side_length = banyan_cmap.sqr();
    int64_t n_rows = banyan_cmap.vnodes() + 1;
    FASSERT(n_rows >= 2)
        (banyan_cmap.vnodes());
    int64_t n_cols = banyan_cmap.hnodes() + 1;
    FASSERT(n_cols >= 2)
        (banyan_cmap.hnodes());
    linden::geom::Point ll(
        banyan_cmap.hoffset() - side_length / 2,
        banyan_cmap.voffset() - side_length / 2);
    linden::geom::Point ur = ll + linden::geom::Point(
        side_length * n_cols,
        side_length * n_rows);
    linden::geom::Rect real_region(ll, ur);
    cmap.setChipCoreRegion(real_region);
    cmap.initGrids(n_cols, n_rows);

    using linden::geom::dimension::X;
    int64_t n_hedges_per_row = n_cols - 2;

    double h_sum_demand = 0;
    double h_sum_supply = 0;
    double v_sum_demand = 0;
    double v_sum_supply = 0;
    for (auto i = 0; i < banyan_cmap.hedges_size(); ++i) {
        auto const& edge = banyan_cmap.hedges(i);
        if (edge.active()) {
            auto w_idx = i % n_hedges_per_row + 1;
            auto h_idx = i / n_hedges_per_row;
            auto const& rect = cmap.rect(w_idx, h_idx);
            linden::geom::Point offset(side_length * w_idx, side_length * h_idx);
            FASSERT(rect.ll() == ll + offset)
                (rect)
                (ll)
                (offset)
                (side_length)
                (w_idx)
                (h_idx);
            cmap.updateSupply(X, w_idx, h_idx, edge.supply() / 2);
            cmap.updateDemand(X, w_idx, h_idx, edge.demand() / 2);
            cmap.updateSupply(X, w_idx, h_idx + 1, edge.supply() / 2);
            cmap.updateDemand(X, w_idx, h_idx + 1, edge.demand() / 2);
            h_sum_demand += edge.demand();
            h_sum_supply += edge.supply();
        }
    }

    using linden::geom::dimension::Y;
    int64_t n_vedges_per_row = n_rows - 2;
    for (auto i = 0; i < banyan_cmap.vedges_size(); ++i) {
        auto const& edge = banyan_cmap.vedges(i);
        if (edge.active()) {
            auto w_idx = i / n_vedges_per_row;
            auto h_idx = i % n_vedges_per_row + 1;
            auto const& rect = cmap.rect(w_idx, h_idx);
            linden::geom::Point offset(side_length * w_idx, side_length * h_idx);
            FASSERT(rect.ll() == ll + offset)
                (rect)
                (ll)
                (offset)
                (side_length)
                (w_idx)
                (h_idx);
            cmap.updateSupply(Y, w_idx, h_idx, edge.supply() / 2);
            cmap.updateDemand(Y, w_idx, h_idx, edge.demand() / 2);
            cmap.updateSupply(Y, w_idx + 1, h_idx, edge.supply() / 2);
            cmap.updateDemand(Y, w_idx + 1, h_idx, edge.demand() / 2);
            v_sum_demand += edge.demand();
            v_sum_supply += edge.supply();
        }
    }
    const bool use_expander = false;
    if (!cmap.updateOverflows(use_expander)) {
        spdlog::info("no overflow in median level");
    }
    const double rc = cmap.getCurRouteCongestionValue();
    spdlog::info("[linden/gpl/rdp] rc = {}, H d/s = {}, V d/s = {}", rc, h_sum_demand / h_sum_supply, v_sum_demand / v_sum_supply);

    placer.load_cmap(cmap);

    return rc;
}

} // namespace

void linden_global_place(const Config& cfg) noexcept {
    if (LINDEN_IN_REGRESSION()) {
        spdlog::set_pattern("%v"); // no timestamp
    }

    spdlog::info("{}", pp::prettyPrint(cfg));

    shared_ptr<linden::db::Database> ldb = build_linden_db();

    unique_ptr<Graphics> graphics;
    if (Graphics::enabled()) {
        graphics.reset(new Graphics(ldb));
    }

    if (!cfg.continue_mode) {
        // make sure linden start with a clean state.
        if (cfg.continuation) {
            if (filesystem::exists(*cfg.continuation)) {
                filesystem::remove(*cfg.continuation);
            }
        }
        initial_place(cfg, *ldb);
    } else {
        if (!cfg.continuation) {
            spdlog::error("[linden/gpl] continue-mode requires a continuation file.");
            return;
        }
        if (!filesystem::exists(*cfg.continuation)) {
            spdlog::error("[linden/gpl] continue-mode requires a continuation file.");
            return;
        }
    }

    unique_ptr<linden::gpl::Placer> gpl;
    bool routability_driven = false;
    if (cfg.routability_driven) {
        if (cfg.highest_layer_to_route) {
            routability_driven = true;
            spdlog::info(
                "[linden/gpl] "
                "enable routability-driven with the highest layer: {}",
                *cfg.highest_layer_to_route);
        } else {
            spdlog::warn(
                "[linden/gpl] "
                "Routability-driven is forcely disabled because "
                "the highest layer is not specifed for (baby) banyan "
                "to estimate routability.");
        }
    }
    {
        linden::gpl::PlacerConfig ncfg;
        if (cfg.density) {
            ncfg.target_density = *cfg.density;
            ncfg.use_uniform_density = false;
        } else {
            ncfg.target_density = 1.0;
            ncfg.use_uniform_density = true;
        }
        ncfg.target_overflow = 0.0;
        ncfg.routability_driven = routability_driven;
        ncfg.use_expander_inflation = false;
        gpl = linden::gpl::Placer::create(ldb, ncfg);
    }

    if (cfg.timing_driven) {
        spdlog::info("[linden/gpl] enable timing-driven");
        ::ord::OpenRoad::openRoad()->getResizer()->resizeSlackPreamble();
    }

    optional<RelevanceDetector> relev;
    if (LINDEN_GP_CELL_RELATIVE_POS_CAL()) {
        relev = move(RelevanceDetector(*ldb));
    }

    // Gp Loop
    size_t gp_iter = 1;
    if (cfg.ldb_path) {
        // gpl->one_step_move(gp_iter);
        gpl->read_db(cfg.ldb_path.value());
        refresh_locations_from_ldb(*ldb);
        if (graphics) {
            graphics->one_more_step(gp_iter);
        }
    }

    size_t best_iter = 0;
    auto ha = HpwlAnalyze();
    bool start_rdgp = false;
    constexpr int64_t GPL_MAX_ITER = 3000;
    deque<double> overflows = {90, 79, 64, 49, 29, 21, 0};
    auto gp_going = [&](){
        const int rdp_count_down_init = LINDEN_GP_CMAP_PERIOD();
        int rdp_count_down = 0;
        for (; !overflows.empty() && gp_iter < GPL_MAX_ITER; gp_iter++) {
            auto ret = gpl->one_step_move(gp_iter);
            spdlog::debug("[gpl] overflow: {:.5f} hpwl: {}", ret.overflow, ret.hpwl);
            if (gp_iter == 1 || gp_iter % 10 == 0) {
                spdlog::info("[nPlace] Iter: {} overflow: {:g} HPWL: {}", gp_iter, ret.overflow, ret.hpwl);
            }

            // Routability Driven
            if (cfg.routability_driven && ret.overflow < cfg.overflow_to_estimate_cmap && !start_rdgp) {
                spdlog::info("[nPlace] Begin Routability Driven.");
                start_rdgp = true;
            }

            if (gp_iter % 100 == 0) {
                auto [cmap_file, ldb_file] = get_tmp_path(gp_iter);
                gpl->write_db(ldb_file);
            }

            if (cfg.routability_driven && start_rdgp) {
                if (rdp_count_down == 0) {
                    // Refresh DataBase
                    best_iter = gp_iter;
                    refresh_locations_from_ldb(*ldb);
                    scanchain_reorder(*ldb);
                    auto [cmap_file, ldb_file] = get_tmp_path(gp_iter);

                    // Stash DataBase
                    gpl->write_db(ldb_file);

                    // Get Cmap
                    estimate_cmap(cmap_file, cfg, ha.get_mm_iter(), ha.get_gcells_size());
                    const double rc = refresh_cmap_to_placer(*gpl, cmap_file);
                    
                    // Check Status
                    ha.push_rc(rc, gp_iter);
                    if (ha.rdgp_shot()) {
                        spdlog::info("[nPlace] take snapshot RC({}).", rc);
                        gpl->take_snapshot();
                    }
                    if (ha.rdgp_blew()) {
                        spdlog::info("[nPlace] restore snapshot minRC({}, {}).", ha._min_rc, ha._min_rc_iter);
                        gpl->restore_snapshot();
                        best_iter = ha._min_rc_iter;
                        refresh_locations_from_ldb(*ldb);
                        return true;
                    }
                    if (ha.rdgp_done()) {
                        spdlog::info("[nPlace] RDGP Got TargetRC.");
                        refresh_locations_from_ldb(*ldb);
                        return true;
                    }
                    if (ha._rc_vec.size() >= LINDEN_GP_CMAP_ITERATION()) {
                        spdlog::info("[nPlace] RDGP Got Max iterations({}).", LINDEN_GP_CMAP_ITERATION());
                        spdlog::info("[nPlace] restore snapshot minRC({}, {}).", ha._min_rc, ha._min_rc_iter);
                        gpl->restore_snapshot();
                        best_iter = ha._min_rc_iter;
                        refresh_locations_from_ldb(*ldb);
                        return true;
                    }

                    rdp_count_down = rdp_count_down_init;
                }
                rdp_count_down --;
            }

            // Target overflow
            if (ret.overflow < cfg.overflow && !start_rdgp) {
                spdlog::info("[nPlace] target overflow.");
                best_iter = gp_iter;
                refresh_locations_from_ldb(*ldb);
                return true;
            }

            // HPWL Analyze
            ha.push(ret.hpwl, ret.overflow);
            if (ha.repulsion_dominance() && !cfg.routability_driven) {
                spdlog::info("[nPlace] Repulsion is dominant.");
                return true;
            }
            if (ha.heat_death()) {
                spdlog::info("[nPlace] Heat death!");
                return true;
            }

            // RelevanceDetector
            if (relev && gp_iter % 10 == 0) {
                auto const& [x_change, y_change] = relev->update(*ldb);
                spdlog::info("[nPlace] relative position change = ({:.2f}%, {:.2f}%)",x_change * 100, y_change * 100);
            }

            // Graphics
            if (graphics) {
                graphics->one_more_step(gp_iter);
            }

            // Netweigthing
            if (cfg.timing_driven && overflows.front() > ret.overflow * 100) {
                while(!overflows.empty() && overflows.front() > ret.overflow * 100) {
                  overflows.pop_front();
                }
                best_iter = gp_iter;
                refresh_locations_from_ldb(*ldb);

                if (overflows.empty()) {
                    spdlog::info("[nPlace] Terminates because overflows are exhausted.");
                    return true;
                } else {
                    spdlog::debug("[nPlace] Current level of overflow is {:.2f}", overflows.front());
                    return false;
                }
            }

            // Stash GPL
            if (ha.best_gpl_postion() && !routability_driven) {
                spdlog::info("[nPlace] best gpl postion: overflow: {}, hpwl: {}",ret.overflow, ret.hpwl);
                best_iter = gp_iter;
                refresh_locations_from_ldb(*ldb);
            }
        }
        return true;
    };

    for(;;) {
        auto gp_done = gp_going();
        if (gp_done) {
            break;
        }
        if (cfg.timing_driven) {
            update_weights(*ldb);
        }
    }
    ha.save_data();

    gpl->done();
    gpl.reset();
    if (graphics) {
        graphics->finalize();
        graphics.reset();
    }

    metrics::Config mcfg = metrics::Config::sink_to_stderr();
    if (auto opt = LINDEN_GP_METRICS_FILE(); opt) {
        mcfg = metrics::Config::sink_to_file(*opt);
    }
    metrics::Reporter metrics_reporter(mcfg);
    metrics_reporter["gp"]["iter"] = best_iter + 1;
}

void random_place() noexcept {
    if (LINDEN_IN_REGRESSION()) {
        spdlog::set_pattern("%v"); // no timestamp
    }

    shared_ptr<linden::db::Database> ldb = build_linden_db();

    unique_ptr<Graphics> graphics;
    if (Graphics::enabled()) {
        graphics.reset(new Graphics(ldb));
    }

    const auto& core = ldb->core_region();
    auto lx = core.ll().x();
    auto ly = core.ll().y();
    auto ux = core.ur().x();
    auto uy = core.ur().y();
    mt19937_64 rng(0);
    uniform_int_distribution<int64_t> x_rng(lx, ux);
    uniform_int_distribution<int64_t> y_rng(ly, uy);
    for (auto& c: ldb->mut_moveable_insts()) {
        auto& cell = c.get();
        int64_t x = x_rng(rng);
        int64_t y = y_rng(rng);
        cell.move_to(linden::geom::Point {x, y});
    }

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
    metrics_reporter["gp"]["iter"] = 0;
}

} // namespace linden::gp
