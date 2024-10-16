#include "linden/dp.hpp"
#include "../util/lefdef_dumper.hpp"
#include "dp.hpp"
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

namespace linden {

DpConfig::DpConfig()
:   suppress_output(false),
    not_stop_when_illegal(false)
{}

int exec(
    const DpConfig& cfg,
    const std::filesystem::path& exe,
    const deque<string>& args)
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
    env1["LINDEN_DP_ALGORITHM"] = string(LINDEN_DP_ALGORITHM());
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
        if (cfg.not_stop_when_illegal) {
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

int exec_basic_dp(
    const DpConfig& cfg,
    const filesystem::path& input_cfg,
    const filesystem::path& outdef)
{
    deque<string> args;
    args.push_back("--input-config");
    args.push_back(input_cfg);
    args.push_back("--to");
    args.push_back(outdef);
    args.push_back("--cell-only");
    auto exe = get_basic_dp_exe();
    return exec(cfg, exe, args);
}

int exec_incr_dp(
    const DpConfig& cfg,
    const string& input_cfg,
    const string& outdef)
{
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
    auto exe = std::move(get_incr_dp_exe());
    return exec(cfg, exe, args);
}

int exec_routability_driven_dp(
    const DpConfig& cfg,
    const string& input_cfg,
    const string& outdef)
{
    deque<string> args;
    args.push_back("--input-config");
    args.push_back(input_cfg);
    args.push_back("--to");
    args.push_back(outdef);
    auto exe = std::move(get_routability_driven_dp_exe());
    return exec(cfg, exe, args);
}

int exec_pbdef(
    const DpConfig& cfg,
    const string& from,
    const string& to)
{
    deque<string> args;
    args.push_back("--from");
    args.push_back(from);
    args.push_back("--to");
    args.push_back(to);
    auto exe = std::move(get_pbdef_exe());
    return exec(cfg, exe, args);
}

filesystem::path write_input_config(
    const DpConfig& cfg,
    const filesystem::path& basename,
    const deque<filesystem::path>& lefs,
    const filesystem::path& def)
{
    auto root = toml::table{
        {"version", "1.2"},
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

void read_back(const filesystem::path& outdef)
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
    rsz::Resizer& resizer = *ord.getResizer();
    resizer.estimateParasitics(rsz::ParasiticsSrc::placement);
}

std::filesystem::path generate_routability_data_file(const string_view& routing_top_layer) {
    banyan::Banyan& banyan = *::ord::OpenRoad::openRoad() -> getBanyan();
    auto cmap_filepath = gen_base_filename() / fmt::format(".cmap");
    banyan.clearWires(false, false);
    banyan.cmap(cmap_filepath.string(), false, routing_top_layer);
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

int basic_dp(::ord::OpenRoad& ord, const DpConfig& cfg) {
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

    filesystem::path input_cfg_fn = write_input_config(cfg, basename, lefs, def);
    filesystem::path outdef = output_def_name(basename, cfg.suppress_output);
    int ret = exec_basic_dp(cfg, input_cfg_fn, outdef);
    read_back(outdef);
    return ret;
}

int incr_dp(::ord::OpenRoad& ord, const DpConfig& cfg) {
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

    filesystem::path input_cfg_fn = write_input_config(cfg, basename, lefs, def);
    filesystem::path outdef = output_def_name(basename, cfg.suppress_output);
    int ret = exec_incr_dp(cfg, input_cfg_fn, outdef);
    read_back(outdef);
    return ret;
}

int rddp(::ord::OpenRoad& ord, DpConfig& dpcfg) {
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

    for (int64_t tried = 0; tried < N_TRIES; tried++) {
        const auto& basename = gen_base_filename();
        filesystem::path input_cfg_fn = write_input_config(dpcfg, basename, lefs, last_def);
        const filesystem::path& out_def = output_def_name(basename, dpcfg.suppress_output);
        int64_t ret = exec_routability_driven_dp(dpcfg, input_cfg_fn, out_def);
        if (ret) {
            spdlog::error(
                "Routability-Driven Detailed Placement failed, ",
                "code: {}",
                ret
            );
            return ret;
        }
        read_back(out_def);

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
    read_back(*best_try);
    spdlog::info("Pick the best solution among {} tries. def_path: {}", tried_results.size(), *best_try);
    return 0;
}

} // namespace linden

