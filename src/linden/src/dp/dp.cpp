#include "linden/dp.hpp"
#include "../util/lefdef_dumper.hpp"
#include "dbTypes.h"
#include "dp.hpp"
#include "geom.h"
#include "linden/eco_placer.hpp"
#include "rsz/Resizer.hh"
#include "lrl/CircuitModifier/CircuitModifier.hh"
#include "lrl/Laurel.hh"
#include "sta/StaMain.hh"
#include "sta/Units.hh"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "toml++/toml.h"
#include "envflag/envflag.hpp"
#include "fassert.hpp"
#include "pblefdef.pb.h"
#include "banyan/Banyan.h"
#include "banyan_cmap.pb.h"
#include "fassert.hpp"
#include "linden/geom/dimension.hpp"
#include <spdlog/spdlog.h>
#include <boost/process.hpp>
#include <exception>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <deque>
#include <string>
#include <utility>
#include <functional>
#include <optional>
#include <cstdlib>
#include <filesystem>

using namespace std;
using linden::geom::Dimension;

IMPORT_STR_FLAG_WITH_DEFAULT(LINDENDP_EXE_DIR);
IMPORT_STR_FLAG_WITH_DEFAULT(LINDEN_DP_ALGORITHM);
IMPORT_BOOL_FLAG_WITH_DEFAULT(LINDEN_IN_REGRESSION);

namespace linden::dp {

Config::Config()
{}

void Config::prettyPrint(string& out) const {
    if (selected_fillers) {
        out.append("fillers: ");
        for(auto const& x: *selected_fillers) {
            out.append(x);
            out.append(", ");
        }
        out.push_back('\n');
    }
    if (timing_data) {
        out.append("timing-data: ");
        out.append(*timing_data);
        out.push_back('\n');
    }
    if (routing_congestion_data) {
        out.append("routing-congestion data: ");
        out.append(*routing_congestion_data);
        out.push_back('\n');
    }
    if (routing_top_layer) {
        out.append("top layer for routing: ");
        out.append(*routing_top_layer);
        out.push_back('\n');
    }
    if (critical_cells) {
        out.append("critical cells: ");
        for(auto const& x: *critical_cells) {
            out.append(x);
            out.append(", ");
        }
        out.push_back('\n');
    }
    if (baseline_def) {
        out.append("baseline: ");
        out.append(*baseline_def);
        out.push_back('\n');
    }
    if (!extra_pg_spacing.empty()) {
        out.append("extra pg spacing:\n");
        for(auto const& [layer, spacing]: extra_pg_spacing) {
            out.append("  ");
            out.append(layer);
            out.push_back('=');
            pp::prettyPrint(out, spacing);
            out.push_back('\n');
        }
    }
    if (suppress_output) {
        out.append("suppress output: ");
        pp::prettyPrint(out, *suppress_output);
        out.push_back('\n');
    }
    if (not_stop_when_illegal) {
        out.append("not stop when illegal: ");
        pp::prettyPrint(out, *not_stop_when_illegal);
        out.push_back('\n');
    }
}

shared_ptr<Config> Config::get_default() noexcept {
    static shared_ptr<Config> _g(new Config());
    return _g;
}

namespace {

template<class T>
void merge(optional<T>& base, const optional<T>& up) {
    if (up) {
        base = up;
    }
}

} // namespace

Config& Config::operator+=(const Config& ano) {
    merge(selected_fillers, ano.selected_fillers);
    merge(timing_data, ano.timing_data);
    merge(routing_congestion_data, ano.routing_congestion_data);
    merge(routing_top_layer, ano.routing_top_layer);
    merge(critical_cells, ano.critical_cells);
    merge(baseline_def, ano.baseline_def);
    merge(suppress_output, ano.suppress_output);
    merge(not_stop_when_illegal, ano.not_stop_when_illegal);
    merge(disable_updating_parasitics, ano.disable_updating_parasitics);
    merge(min_layer, ano.min_layer);
    merge(max_layer, ano.max_layer);
    if (!ano.extra_pg_spacing.empty()) {
        for(auto const& [layer, spacing]: ano.extra_pg_spacing) {
            extra_pg_spacing[layer] = spacing;
        }
    }
    return *this;
}

int exec(
    const Config& cfg,
    const std::filesystem::path& exe,
    const deque<string>& args,
    const string_view& algorithm)
{
    if (!std::filesystem::exists(exe)) {
        spdlog::error("Cannot find {}", exe);
        throw std::runtime_error("fail to run lindendp");
    }
    using std::filesystem::perms;
    if (auto status = std::filesystem::status(exe);
        (status.permissions() & (perms::owner_exec | perms::group_exec | perms::others_exec)) == perms::none)
    {
        spdlog::error("Cannot execute {}", exe);
        throw std::runtime_error("fail to run lindendp");
    }
    int ec = 0;
    auto env = boost::this_process::environment();
    boost::process::environment env1 = env;
    env1.set("LINDEN_DP_ALGORITHM", string(algorithm));
    if (!cfg.suppress_output) {
        ec = boost::process::system(string(exe), boost::process::args(args), env1);
    } else {
        ec = boost::process::system(string(exe), boost::process::args(args),
            env1,
            boost::process::std_out > boost::process::null,
            boost::process::std_err > boost::process::null);
    }
    switch (ec)
    {
    case 0:
        // intend to do nothing
        return ec;
    case 1:
        spdlog::warn("Cannot legalize completely.");
        if (cfg.not_stop_when_illegal.value_or(false)) {
            spdlog::info("But not to stop the flow.");
            return ec;
        }
        break;
    default:
        break;
    }
    spdlog::warn("`lindendp` exit code: {}", ec);
    throw std::runtime_error("fail to run lindendp");
}

odb::dbOrientType pb_to_dbOrientType(int pb) noexcept
{
    pblefdef::Orient orient = static_cast<pblefdef::Orient>(pb);
    switch (orient) {
        case pblefdef::Orient::NORTH: return odb::dbOrientType::R0;
        case pblefdef::Orient::SOUTH: return odb::dbOrientType::R180;
        case pblefdef::Orient::EAST: return odb::dbOrientType::R270;
        case pblefdef::Orient::WEST: return odb::dbOrientType::R90;
        case pblefdef::Orient::FLIPPED_NORTH: return odb::dbOrientType::MY;
        case pblefdef::Orient::FLIPPED_SOUTH: return odb::dbOrientType::MX;
        case pblefdef::Orient::FLIPPED_EAST: return odb::dbOrientType::MYR90;
        case pblefdef::Orient::FLIPPED_WEST: return odb::dbOrientType::MXR90;
        default: {
            FASSERT(false).what("unreachable!");
            return odb::dbOrientType::R0;
        }
    }
}

filesystem::path output_def_name(
    const filesystem::path& basename,
    bool suppress_output)
{
    filesystem::path res = basename;
    res += ".out.pbdef";
    if (!suppress_output) {
        cout << "out def: " << res << endl;
    }
    return res;
}

namespace {

std::filesystem::path get_linden_dp_exe() {
    std::filesystem::path dir(LINDENDP_EXE_DIR());
    return dir / "lindendp";
}

std::filesystem::path get_linden_incr_dp_exe() {
    std::filesystem::path dir(LINDENDP_EXE_DIR());
    return dir / "linden-incremental-legalizer";
}

std::filesystem::path get_basic_dp_exe() {
    std::filesystem::path dir(LINDENDP_EXE_DIR());
    string exec;
    if (LINDEN_DP_ALGORITHM() == "deblais"sv) {
        exec = "deblais-dp";
    } else {
        exec = "lindendp";
    };
    return dir / exec;
}

std::filesystem::path get_incr_dp_exe() {
    std::filesystem::path dir(LINDENDP_EXE_DIR());
        string exec;
    if (LINDEN_DP_ALGORITHM() == "deblais"sv) {
        exec = "deblais-incrdp";
    } else {
        exec = "linden-incremental-legalizer";
    };
    return dir / exec;
}

std::filesystem::path get_routability_driven_dp_exe() {
    std::filesystem::path dir(LINDENDP_EXE_DIR());
    return dir / "linden-routability-driven-detailed-placer";
}

std::filesystem::path get_pbdef_exe() {
    std::filesystem::path dir(LINDENDP_EXE_DIR());
    return dir / "pbdef";
}

std::filesystem::path get_opt_exe() {
    std::filesystem::path dir(LINDENDP_EXE_DIR());
    return dir / "lindenopt";
}

filesystem::path write_input_config(
    const Config& cfg,
    const filesystem::path& basename,
    const deque<filesystem::path>& lefs,
    const filesystem::path& def)
{
    auto root = toml::table{
        {"$schema", "https://icbench.com/schemas/linden/input-cfg/1.3.0"},
        {"version", "1.3.0"}, // compatible to deblais
    };
    {
        toml::array lefs_;
        for(auto const& lef: lefs) {
            lefs_.push_back(lef.string());
        }
        root.emplace("lefs", lefs_);
    }
    {
        toml::array defs_;
        defs_.push_back(def.string());
        root.emplace("defs", defs_);
    }
    if (cfg.selected_fillers) {
        toml::array fillers;
        for(auto const& f: *cfg.selected_fillers) {
            fillers.push_back(f);
        }
        auto mid = toml::table{
            {"list", fillers},
        };
        root.emplace("fillers", mid);
    }
    if (auto const& td = cfg.timing_data; td) {
        root.emplace("timing-data", td->string());
    }
    if (auto const& td = cfg.routing_congestion_data; td) {
        root.emplace("routability-data", td->string());
    }
    if (auto const& cc = cfg.critical_cells; cc) {
        toml::array critical_cells;
        for(auto const& c: *cc) {
            critical_cells.push_back(c);
        }
        root.emplace("critical-cells", critical_cells);
    }
    if(auto const& pgs = cfg.extra_pg_spacing; !pgs.empty()) {
        toml::table extra_pg_spacing;
        for(auto const& [layer, spacing]: pgs) {
            toml::value v(spacing);
            extra_pg_spacing.insert_or_assign(layer, v);
        }
        root.emplace("extra-spacing-for-pg", extra_pg_spacing);
    }

    filesystem::path filename = basename;
    filename += ".toml";
    {
        ofstream fout(filename);
        fout << root;
        fout.close();
    }
    if (!cfg.suppress_output) {
        cout << "input-cfg: " << filename << endl;
    }
    return filename;
}

int exec_basic_dp(
    const Config& cfg,
    const filesystem::path& input_cfg,
    const filesystem::path& outdef,
    const filesystem::path& basename,
    const deque<filesystem::path>& lefs
) {
    if (LINDEN_DP_ALGORITHM() == "deblais"sv) {
        deque<string> args;
        args.push_back("--input-config");
        args.push_back(input_cfg);
        args.push_back("--to");
        args.push_back(outdef);
        auto exe = get_basic_dp_exe();
        int ret = exec(cfg, exe, args, LINDEN_DP_ALGORITHM());
        if (ret) {
            return ret;
        }
        std::filesystem::path pa_incfg = write_input_config(cfg, basename, lefs, outdef);
        args.clear();
        args.push_back("--input-config");
        args.push_back(pa_incfg);
        args.push_back("--to");
        args.push_back(outdef);
        args.push_back("--cell-only");
        return exec(cfg, get_linden_dp_exe(), args, "klotski"sv);
    } else {
        deque<string> args;
        args.push_back("--input-config");
        args.push_back(input_cfg);
        args.push_back("--to");
        args.push_back(outdef);
        args.push_back("--cell-only");
        auto exe = get_basic_dp_exe();
        return exec(cfg, exe, args, LINDEN_DP_ALGORITHM());
    }
}

int exec_incr_dp(
    const Config& cfg,
    const string& input_cfg,
    const string& outdef,
    const filesystem::path& basename,
    const deque<filesystem::path>& lefs
)
{
    if (LINDEN_DP_ALGORITHM() == "deblais"sv) {
        deque<string> args;
        args.push_back("--input-config");
        args.push_back(input_cfg);
        args.push_back("--to");
        args.push_back(outdef);
        if (cfg.baseline_def) {
            args.push_back("--baseline");
            args.push_back(*cfg.baseline_def);
        }
        auto exe = get_incr_dp_exe();
        int ret = exec(cfg, exe, args, LINDEN_DP_ALGORITHM());
        if (ret) {
            return ret;
        }
        std::filesystem::path pa_incfg = write_input_config(cfg, basename, lefs, outdef);
        args.clear();
        args.push_back("--input-config");
        args.push_back(pa_incfg);
        args.push_back("--to");
        args.push_back(outdef);
        args.push_back("--cell-only");
        if (cfg.baseline_def) {
            args.push_back("--baseline");
            args.push_back(*cfg.baseline_def);
        }
        if (cfg.min_layer.has_value() && cfg.max_layer.has_value()) {
            args.emplace_back("--tech-lef");
            auto banyan = ord::OpenRoad::openRoad()->getBanyan();
            auto techlef = banyan->getTechlef();
            auto techlefs = banyan->getOtherTechlefs();
            for (const auto &techlef: techlefs) {
                args.emplace_back(techlef);
            }
            args.emplace_back(techlef);
            auto cell_lefs = banyan->getLiblefs();
            args.emplace_back("--cell-lef");
            for (const auto &x: cell_lefs) {
                args.emplace_back(x);
            }
            args.emplace_back("--min-layer");
            args.emplace_back(cfg.min_layer->c_str());
            args.emplace_back("--max-layer");
            args.emplace_back(cfg.max_layer->c_str());
        }
        return exec(cfg, get_linden_incr_dp_exe(), args, "klotski"sv);
    } else {
        deque<string> args;
        args.push_back("--input-config");
        args.push_back(input_cfg);
        args.push_back("--to");
        args.push_back(outdef);
        args.push_back("--cell-only");
        if (cfg.baseline_def) {
            args.push_back("--baseline");
            args.push_back(*cfg.baseline_def);
        }
        auto exe = get_incr_dp_exe();
        return exec(cfg, exe, args, LINDEN_DP_ALGORITHM());
    }
}

int exec_routability_driven_dp(
    const Config& cfg,
    const string& input_cfg,
    const string& outdef)
{
    deque<string> args;
    args.push_back("--input-config");
    args.push_back(input_cfg);
    args.push_back("--to");
    args.push_back(outdef);
    auto exe = get_routability_driven_dp_exe();
    return exec(cfg, exe, args, LINDEN_DP_ALGORITHM());
}

int exec_pbdef(
    const Config& cfg,
    const string& from,
    const string& to)
{
    deque<string> args;
    args.push_back("--from");
    args.push_back(from);
    args.push_back("--to");
    args.push_back(to);
    auto exe = get_pbdef_exe();
    return exec(cfg, exe, args, LINDEN_DP_ALGORITHM());
}

int exec_opt(
    const Config& cfg,
    const string& input_cfg,
    const string& outdef)
{
    deque<string> args;
    args.push_back("--input-config");
    args.push_back(input_cfg);
    args.push_back("--to");
    args.push_back(outdef);
    args.push_back("--cell-only");
    auto exe = get_opt_exe();
    return exec(cfg, exe, args, LINDEN_DP_ALGORITHM());
}
void read_back(const filesystem::path& outdef, const bool disable_updating_parasitics)
{
    pblefdef::DesignExchangeFormat def;
    {
        ifstream fin(outdef);
        if (!fin) {
            spdlog::error("Cannot open result DEF from Linden detailed placer: {}", outdef);
            throw std::runtime_error("Cannot open result DEF from Linden detailed placer.");
        }
        bool ret = def.ParseFromIstream(&fin);
        if (!ret) {
            spdlog::error("Cannot parse result DEF from Linden detailed placer: {}", outdef);
            throw std::runtime_error("Cannot parse result DEF from Linden detailed placer.");
        }
    }
    ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
    lrl::NetworkModifier& cm = lrl::Laurel::getPlacementNetworkModifier();
    lrl::STANetwork sta_nw = cm.network;
    odb::dbBlock& top = *(ord.getDb()->getChip()->getBlock());
    int dbu = top.getDefUnits();
    int64_t pm_per_dbu = 1'000'000 / dbu;
    for(size_t i = 0, sz = def.components_size(); i < sz; ++i) {
        const pblefdef::DesignExchangeFormat_Component& comp = def.components(i);
        const string& cell_name = comp.name();
        FASSERT(comp.has_place());
        const pblefdef::DesignExchangeFormat_Place& place = comp.place();
        FASSERT(place.has_position());
        const pblefdef::Point& target_pos = place.position();
        odb::Point target_pt(target_pos.x() / pm_per_dbu, target_pos.y() / pm_per_dbu); // pm to dbu
        odb::dbOrientType target_orient(pb_to_dbOrientType(place.orient()));

        sta::Instance* sta_inst = sta_nw.findInstance(cell_name.c_str());
        FASSERT(sta_inst != nullptr)
            .what("Linden DP so far does not add/remove cells. So, everything must be there in ODB & STA.");

        cm.moveInst(sta_inst, target_pt, target_orient.getValue());
    }
    if (!disable_updating_parasitics) {
        rsz::Resizer& resizer = *ord.getResizer();
        resizer.estimateParasitics(rsz::ParasiticsSrc::placement);
    }
}

std::filesystem::path generate_routability_data_file(const string_view& routing_top_layer) {
    banyan::Banyan& banyan = *::ord::OpenRoad::openRoad() -> getBanyan();
    auto cmap_filepath = gen_base_filename() / fmt::format(".cmap");
    banyan.clearWires(false, false);
    banyan.cmap(cmap_filepath.string(), routing_top_layer);
    return cmap_filepath;
}

double calc_fulfill(double demand, double supply) noexcept {
    if (supply == 0) {
        return 100; // an arbitrary but large enough value
    }
    return demand / supply;
}

optional<banyan::Cmap> parse_cmap(const filesystem::path& cmap_file)
{
    banyan::Cmap cmap;
    ifstream input(cmap_file, ios::binary);
    if (!input) {
        spdlog::error("Failed to open file: {}", cmap_file);
        return nullopt;
    } else if (!cmap.ParseFromIstream(&input)) {
        spdlog::error("Failed to parse cmap file: {}", cmap_file);
        return nullopt;
    }
    return cmap;
}

int64_t is_congestion_meet_expectation(const banyan::Cmap& cmap)
{
    std::array<std::vector<double>, 2> rc_vals_xy;
    for (auto dim : {Dimension::X, Dimension::Y}) {
        auto& rc_vals = rc_vals_xy[dim];
        rc_vals.reserve(cmap.wgrids() * cmap.hgrids());
        auto const& tiles = (dim == Dimension::X ? cmap.htiles() : cmap.vtiles());
        for (auto const& tile: tiles) {
            rc_vals.push_back(calc_fulfill(tile.demand(), tile.supply()));
        }
        std::sort(rc_vals.begin(), rc_vals.end(), greater<double>());
        double median = rc_vals[rc_vals.size() / 2];
        double max = rc_vals.front();
        if (max > median * 1.2 && max > 1.0) {
            int64_t n_tile_overloaded =
                upper_bound(rc_vals.begin(), rc_vals.end(), 1.0l, std::greater<double>()) - rc_vals.begin();
            return n_tile_overloaded;
        }
    }
    return 0;
}
} // namespace

int basic_dp(::ord::OpenRoad& ord, const Config& cfg) {
    legalize_spare_cell(ord);
    
    const LefDefDumper dumper(ord);
    if (!cfg.suppress_output) {
        dumper.print_dumped_files(std::cout);
    }

    auto const& basename = dumper.basename;
    deque<filesystem::path> lefs;
    lefs.push_back(dumper.tech_lef);
    for(auto const& x: dumper.lib_lefs) {
        lefs.push_back(x);
    }
    auto const& def = dumper.def;

    bool suppress_output = cfg.suppress_output.value_or(false);
    filesystem::path input_cfg_fn = write_input_config(cfg, basename, lefs, def);
    filesystem::path outdef = output_def_name(basename, suppress_output);
    int ret = exec_basic_dp(cfg, input_cfg_fn, outdef, basename, lefs);
    {
        bool disable_updating_parasitics = cfg.disable_updating_parasitics.value_or(false);
        read_back(outdef, disable_updating_parasitics);
    }
    return ret;
}

int incr_dp(::ord::OpenRoad& ord, const Config& cfg) {
    const LefDefDumper dumper(ord);
    if (!cfg.suppress_output) {
        dumper.print_dumped_files(std::cout);
    }

    auto const& basename = dumper.basename;
    deque<filesystem::path> lefs;
    lefs.push_back(dumper.tech_lef);
    for(auto const& x: dumper.lib_lefs) {
        lefs.push_back(x);
    }

    auto const& def = dumper.def;

    bool suppress_output = cfg.suppress_output.value_or(false);
    filesystem::path input_cfg_fn = write_input_config(cfg, basename, lefs, def);
    filesystem::path outdef = output_def_name(basename, suppress_output);
    int ret = exec_incr_dp(cfg, input_cfg_fn, outdef, basename, lefs);
    {
        bool disable_updating_parasitics = cfg.disable_updating_parasitics.value_or(false);
        read_back(outdef, disable_updating_parasitics);
    }
    return ret;
}

int rddp(::ord::OpenRoad& ord, Config& dpcfg) {
    const int64_t N_TRIES = 3;
    std::vector<pair<std::filesystem::path, int64_t>> tried_results;
    tried_results.reserve(N_TRIES + 1);

    const LefDefDumper dumper(ord);
    auto const& basename = dumper.basename;
    filesystem::path last_def = basename;
    last_def += ".pbdef";
    exec_pbdef(dpcfg, dumper.def, last_def);

    deque<filesystem::path> lefs;
    lefs.push_back(dumper.tech_lef);
    for (auto const& x: dumper.lib_lefs) {
        lefs.push_back(x);
    }

    dpcfg.routing_congestion_data = generate_routability_data_file(*dpcfg.routing_top_layer);
    auto cmap = parse_cmap(*dpcfg.routing_congestion_data);
    if (!cmap.has_value()) {
        return 1;
    }

    int64_t n_overloaded_tiles = is_congestion_meet_expectation(*cmap);
    if (!n_overloaded_tiles) {
        return 0;
    } else {
        const filesystem::path& def = last_def;
        tried_results.push_back(make_pair(def, n_overloaded_tiles));
    }

    bool suppress_output = dpcfg.suppress_output.value_or(false);
    for (int64_t tried = 0; tried < N_TRIES; tried++) {
        const auto& basename = gen_base_filename();
        filesystem::path input_cfg_fn = write_input_config(dpcfg, basename, lefs, last_def);
        const filesystem::path& out_def = output_def_name(basename, suppress_output);
        int64_t ret = exec_routability_driven_dp(dpcfg, input_cfg_fn, out_def);
        if (ret) {
            spdlog::error(
                "Routability-Driven Detailed Placement failed, ",
                "code: {}",
                ret
            );
            return ret;
        }
        read_back(out_def, false);

        dpcfg.routing_congestion_data = generate_routability_data_file(*dpcfg.routing_top_layer);
        auto cmap = parse_cmap(*dpcfg.routing_congestion_data);
        if (!cmap.has_value()) {
            return 1;
        }
        int64_t n_overloaded_tiles = is_congestion_meet_expectation(*cmap);
        if (!n_overloaded_tiles) {
            tried_results.push_back(make_pair(last_def,0));
            break;
        } else {
            const filesystem::path& def = last_def;
            tried_results.push_back(make_pair(def, n_overloaded_tiles));
        }
        last_def = out_def;
    }

    optional<std::filesystem::path> best_try;
    int64_t n_overloaded_in_best_try = INT64_MAX;
    for (auto [path, n_overloaded_tiles]: tried_results) {
        if (n_overloaded_in_best_try > n_overloaded_tiles) {
            n_overloaded_in_best_try = n_overloaded_tiles;
            best_try = path;
        }
    }
    // `read_back` will discard cell halos added by linden RDDP.
    // This behaviour is by design because RDDP consider cell halos slightly different with what the LEF/DEF standard defines.
    // Suppose that a tapcell appears in the halo of a cell. Standard may consider it as a violation but RDDP thinks it is okay.
    // So we discard halos added by RDDP, in order to make placement checkers happy.
    read_back(*best_try, false);
    spdlog::info("Pick the best solution among {} tries. def_path: {}", tried_results.size(), *best_try);
    return 0;
}

int opt(::ord::OpenRoad& ord, const Config& cfg) {
    const LefDefDumper dumper(ord);
    if (!cfg.suppress_output) {
        dumper.print_dumped_files(std::cout);
    }

    auto const& basename = dumper.basename;
    deque<filesystem::path> lefs;
    lefs.push_back(dumper.tech_lef);
    for (auto const& x:dumper.lib_lefs) {
        lefs.push_back(x);
    }
    auto const& def = dumper.def;

    bool suppress_output = cfg.suppress_output.value_or(false);
    filesystem::path input_cfg_fn = write_input_config(cfg, basename, lefs, def);
    filesystem::path outdef = output_def_name(basename, suppress_output);
    int ret = exec_opt(cfg, input_cfg_fn, outdef);
    read_back(outdef, false);
    return ret;
}

static odb::Rect row_field(::ord::OpenRoad& ord) {
    std::optional<odb::Rect> res;
    for(auto row : ord.getDb()->getChip()->getBlock()->getRows()) { 
        odb::Rect rect = row->getBBox();
        if (res) {
            res->merge(rect);
        } else {
            res = rect;
        }
    }

    return res.value();
}

int legalize_spare_cell(::ord::OpenRoad& ord)
{
    if (LINDEN_IN_REGRESSION()) {
        spdlog::set_pattern("%v");
    }

    odb::dbBlock* block = ord.getDb()->getChip()->getBlock();
    lrl::NetworkModifier& cm = lrl::Laurel::getPlacementNetworkModifier();
    lrl::STANetwork nw = cm.network;

    int move_spare_cell_count = 0;
    auto row = row_field(ord);
    for (odb::dbGroup* group : block->getGroups()) {
        odb::dbRegion* region = group->getRegion();
        odb::dbSet<odb::dbBox> boxes = region->getBoundaries();
        FASSERT(boxes.size() >= 1);

        spdlog::debug("region: {}, {}", region->getName(), group->getInsts().size());
        for (odb::dbInst* inst : group->getInsts()) {
            auto sta_inst = nw.inst(inst);
            spdlog::debug("  cell: {} {}", nw.name(sta_inst), inst->getPlacementStatus());

            if (inst->getPlacementStatus() == odb::dbPlacementStatus::NONE
                || inst->getPlacementStatus() == odb::dbPlacementStatus::UNPLACED) {
                move_spare_cell_count++;
                inst->setPlacementStatus(odb::dbPlacementStatus::PLACED);
                cm.moveInst(sta_inst, row.ll(), odb::dbOrientType::DEFAULT);
            }
        }
    }

    if (!move_spare_cell_count) {
        return 0;
    }

    spdlog::info("[linden/dp] Spare Cell count: {}", move_spare_cell_count);
    spdlog::info("[linden/dp] Group Count: {}", block->getGroups().size());

    EcoPlacer ep;
    for (odb::dbGroup* group : block->getGroups()) {
        odb::dbRegion* region = group->getRegion();
        odb::dbSet<odb::dbBox> boxes = region->getBoundaries();
        spdlog::debug("Eco region: {}, {}", region->getName(), group->getInsts().size());

        for (odb::dbInst* inst : group->getInsts()) {
            auto sta_inst = nw.inst(inst);
            auto master = nw.name(sta_inst);
            const char* libcell = nw.name(nw.cell(sta_inst));
            auto ret = ep.find_place(libcell, boxes);

            // ep.remove_cell(master);
            inst->setPlacementStatus(odb::dbPlacementStatus::PLACED);
            if (ret) {
                cm.moveInst(sta_inst, ret->point, ret->orient);
                ep.insert_cell(master, libcell, ret.value());
            } else {
                FASSERT(boxes.size() >= 1);
                auto first = *boxes.begin();
                cm.moveInst(sta_inst, first->getBox().center(), odb::dbOrientType::DEFAULT);
            }
        }
    }

    return 0;
}

int check_spare_cell(::ord::OpenRoad& ord)
{
    if (LINDEN_IN_REGRESSION()) {
        spdlog::set_pattern("%v");
    }

    odb::dbBlock* block = ord.getDb()->getChip()->getBlock();

    int out_region_cell = 0;
    for (odb::dbGroup* group : block->getGroups()) {
        odb::dbRegion* region = group->getRegion();
        odb::dbSet<odb::dbBox> boxes = region->getBoundaries();

        for (odb::dbInst* inst : group->getInsts()) {
            odb::Rect rect = inst->getBBox()->getBox();
            bool inAnyBox = false;

            for (auto box : boxes) {
                if (box->getBox().contains(rect)) {
                    inAnyBox = true;
                    break;
                }
            }
            if (!inAnyBox) {
                out_region_cell ++;
            }
        }
    }

    if (out_region_cell) {
        spdlog::info("{} Cells are not fully contained in regions.", out_region_cell);
        return 1;    
    }
    spdlog::info("All spare cells are fully contained in their corresponding regions.");
    return 0;
}

} // namespace linden::dp

