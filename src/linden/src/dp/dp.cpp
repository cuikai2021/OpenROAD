#include "linden/dp.hpp"
#include "../util/lefdef_dumper.hpp"
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

IMPORT_STR_FLAG_WITH_DEFAULT(LINDENDP_EXE_DIR);

namespace linden {

DpConfig::DpConfig()
:   suppress_output(false),
    not_stop_when_illegal(false)
{}

namespace {

std::filesystem::path get_basic_dp_exe() {
    std::filesystem::path dir(LINDENDP_EXE_DIR());
    return dir / "lindendp";
}

std::filesystem::path get_incr_dp_exe() {
    std::filesystem::path dir(LINDENDP_EXE_DIR());
    return dir / "linden-incremental-legalizer";
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
    if (!cfg.suppress_output) {
        ec = boost::process::system(string(exe), boost::process::args(args));
    } else {
        ec = boost::process::system(string(exe), boost::process::args(args),
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

odb::dbOrientType::Value pb_to_dbOrientType(pblefdef::Orient pb) noexcept
{
    switch (pb) {
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

} // namespace linden

