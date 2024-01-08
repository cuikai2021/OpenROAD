#include "linden/lindendp.hpp"
#include "fassert.hpp"
#include "linden/lefdef_dumper.hpp"
#include "pblefdef.pb.h"
#include "toml++/toml.h"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "rsz/Resizer.hh"
#include "lrl/CircuitModifier/CircuitModifier.hh"
#include "lrl/Laurel.hh"
#include "sta/StaMain.hh"
#include "sta/Units.hh"
#include "utl/metrics.hpp"
#include "envflag/envflag.hpp"
#include <spdlog/spdlog.h>
#include <boost/process.hpp>
#include <exception>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <deque>
#include <string>
#include <tuple>
#include <utility>
#include <functional>
#include <optional>
#include <tcl.h>
#include <cstdlib>
#include <string_view>
#include <filesystem>

using namespace std;

IMPORT_STR_FLAG(LINDEN_DP_METRICS_FILE);
IMPORT_STR_FLAG_WITH_DEFAULT(LINDENDP_EXE_DIR);

extern "C" {
extern int Linden_Init(Tcl_Interp *interp);
} // extern "C"

namespace sta {
extern const char* linden_tcl_inits[];
}

namespace linden {

void initLindenDp(::ord::OpenRoad* ord) {
    Tcl_Interp* interp = ord->tclInterp();
    Linden_Init(interp);
    sta::evalTclInit(interp, sta::linden_tcl_inits);
}


DetailedPlacer::DetailedPlacer(::ord::OpenRoad& ord)
  : openroad_(ord)
{}

IncrLegalizer::IncrLegalizer(::ord::OpenRoad& ord)
:   openroad_(ord)
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

string output_def_name(const string& basename) {
    string res(basename);
    res.append(".out.pbdef");
    cout << "out def: " << res << endl;
    return res;
}

int exec(
    const std::filesystem::path& exe,
    const deque<string>& args,
    bool not_stop_when_illegal)
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
    int ec = boost::process::system(string(exe), boost::process::args(args));
    switch (ec)
    {
    case 0:
        // intend to do nothing
        return ec;
    case 1:
        spdlog::warn("Cannot legalize completely.");
        if (not_stop_when_illegal) {
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
    const string& input_cfg,
    const string& outdef,
    bool not_stop_when_illegal)
{
    deque<string> args;
    args.push_back("--input-config");
    args.push_back(input_cfg);
    args.push_back("--to");
    args.push_back(outdef);
    args.push_back("--cell-only");
    auto exe = std::move(get_basic_dp_exe());
    return exec(exe, args, not_stop_when_illegal);
}

int exec_incr_dp(
    const string& input_cfg,
    const string& outdef,
    const optional<string>& baseline,
    bool not_stop_when_illegal)
{
    deque<string> args;
    args.push_back("--input-config");
    args.push_back(input_cfg);
    args.push_back("--to");
    args.push_back(outdef);
    args.push_back("--cell-only");
    if (baseline) {
        args.push_back("--baseline");
        args.push_back(*baseline);
    }
    auto exe = std::move(get_incr_dp_exe());
    return exec(exe, args, not_stop_when_illegal);
}

struct InputArgs
{
    deque<string> lefs;
    string def;
    optional<deque<string>> selected_fillers;
    optional<string> timing_data;
    optional<deque<string>> critical_cells;
};

string write_input_config(
    const InputArgs& args,
    const string& basename)
{
    auto root = toml::table{
        {"version", "1.2"},
    };
    {
        toml::array lefs;
        for(auto const& lef: args.lefs) {
            lefs.push_back(lef);
        }
        root.emplace("lefs", lefs);
    }
    {
        toml::array defs;
        defs.push_back(args.def);
        root.emplace("defs", defs);
    }
    if (args.selected_fillers) {
        toml::array fillers;
        for(auto const& f: *args.selected_fillers) {
            fillers.push_back(f);
        }
        auto mid = toml::table{
            {"list", fillers},
        };
        root.emplace("fillers", mid);
    }
    if (auto const& td = args.timing_data; td) {
        root.emplace("timing-data", *td);
    }
    if (auto const& cc = args.critical_cells; cc) {
        toml::array critical_cells;
        for(auto const& c: *cc) {
            critical_cells.push_back(c);
        }
        root.emplace("critical-cells", critical_cells);
    }

    string filename = basename;
    filename.append(".toml");
    ofstream fout(filename);
    fout << root;
    fout.close();
    cout << "input-cfg: " << filename << endl;
    return filename;
}

void skip_whitespaces(string_view& str, const string_view& whitespace) noexcept {
    size_t n = str.find_first_not_of(whitespace);
    if (n == string_view::npos) {
        str = string_view();
    } else {
        str = str.substr(n);
    }
}

string_view nonwhitespaces(const string_view& str, const string_view& whitespace) noexcept {
    size_t n = str.find_first_of(whitespace);
    if (n == string_view::npos) {
        return str;
    } else {
        return str.substr(0, n);
    }
}

deque<string> split(const string& raw, const string_view& seps) noexcept {
    deque<string> res;
    string_view view(raw);
    for(;;) {
        skip_whitespaces(view, seps);
        if (view.empty()) {
            break;
        }
        auto const& r = nonwhitespaces(view, seps);
        FASSERT(!r.empty())
            (view);
        res.push_back(string(r));
        view = view.substr(r.length());
    }
    return res;
}

char const WHITESPACES[] = " \t\n";

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

void read_back(const string& outdef)
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
    sta::dbNetwork& db_network = *ord.getDbNetwork();
    sta::dbSta& sta = *ord.getSta();
    sta::Network& sta_network = *sta.network();
    lrl::NetworkModifier cm(&ord);
    for(size_t i = 0, sz = def.components_size(); i < sz; ++i) {
        const pblefdef::DesignExchangeFormat_Component& comp = def.components(i);
        const string& cell_name = comp.name();
        FASSERT(comp.has_place());
        const pblefdef::DesignExchangeFormat_Place& place = comp.place();
        FASSERT(place.has_position());
        const pblefdef::Point& target_pos = place.position();
        odb::Point target_pt(target_pos.x() / 1000, target_pos.y() / 1000); // pm to nm
        odb::dbOrientType target_orient(pb_to_dbOrientType(place.orient()));

        sta::Instance* sta_inst = sta_network.findInstance(cell_name.c_str());
        FASSERT(sta_inst != nullptr)
            .what("Linden DP so far does not add/remove cells. So, everything must be there in ODB & STA.");
        odb::dbInst* odb_inst = db_network.staToDb(sta_inst);
        FASSERT(odb_inst != nullptr)
            .what("Linden DP so far does not add/remove cells. So, everything must be there in ODB & STA.");
        odb::dbMaster* master = odb_inst->getMaster();
        odb::Rect bbox;
        master->getPlacementBoundary(bbox);
        odb::dbTransform t(target_orient);
        t.apply(bbox);
        odb::Point real_target_pt(
            target_pt.x() - bbox.xMin(),
            target_pt.y() - bbox.yMin());

        cm.moveInst(sta_inst, real_target_pt, target_orient.getValue());
    }
    rsz::Resizer& resizer = *ord.getResizer();
    resizer.estimateParasitics(rsz::ParasiticsSrc::placement);
}

} // namespace

int DetailedPlacer::go(deque<string>& args)
{
    InputArgs input_cfg;
    optional<string> filler_list = nullopt;
    bool not_stop_when_illegal = false;
    for(; !args.empty(); args.pop_front()) {
        const auto& s = args.front();
        if (strcmp(s.c_str(), "--filler-list") == 0) {
            args.pop_front();
            if (args.empty()) {
                spdlog::error(
                    "Required a list of fillers, separated by whitespaces, "
                    "e.g,. \"FILL1 FILL2\".");
            }
            filler_list = args.front();
        } else if (strcmp(s.c_str(), "--not-to-stop-when-illegal") == 0) {
            not_stop_when_illegal = true;
        } else if (strcmp(s.c_str(), "--timing-data") == 0) {
            args.pop_front();
            input_cfg.timing_data = args.front();
        } else {
            spdlog::info("ignore arg: {}", s);
        }
    }

    if (filler_list) {
        input_cfg.selected_fillers = std::move(split(*filler_list, WHITESPACES));
    }

    dp::LefDefDumper dumper;
    dumper.run(openroad_);
    dumper.print_dumped_files(std::cout);

    string basename = dumper.basename();

    input_cfg.def = dumper.def();
    string tech = dumper.tech_lef();
    input_cfg.lefs = dumper.lib_lefs();
    input_cfg.lefs.push_front(tech);

    string input_cfg_fn = std::move(write_input_config(input_cfg, basename));
    string outdef = std::move(output_def_name(basename));
    int ret = exec_basic_dp(input_cfg_fn, outdef, not_stop_when_illegal);
    read_back(outdef);
    return ret;
}

int IncrLegalizer::go(deque<string>& args)
{
    InputArgs input_cfg;
    optional<string> filler_list = nullopt;
    bool not_stop_when_illegal = false;
    optional<string> baseline = nullopt;
    optional<string> critical_cells = nullopt;
    for(; !args.empty(); args.pop_front()) {
        const auto& s = args.front();
        if (strcmp(s.c_str(), "--filler-list") == 0) {
            args.pop_front();
            if (args.empty()) {
                spdlog::error(
                    "Required a list of fillers, separated by whitespaces, "
                    "e.g,. \"FILL1 FILL2\".");
            }
            filler_list = args.front();
        } else if (strcmp(s.c_str(), "--not-to-stop-when-illegal") == 0) {
            not_stop_when_illegal = true;
        } else if (strcmp(s.c_str(), "--timing-data") == 0) {
            args.pop_front();
            input_cfg.timing_data = args.front();
        } else if (strcmp(s.c_str(), "--baseline") == 0) {
            args.pop_front();
            if (args.empty()) {
                spdlog::error(
                    "Required a DEF file as the baseline.");
            }
            baseline = args.front();
        } else if (strcmp(s.c_str(), "--critical-cells") == 0) {
            args.pop_front();
            if (args.empty()) {
                spdlog::error(
                    "Required a list of critical cells, separated by whitespaces, "
                    "e.g,. \"cell0 cell1\".");
            }
            critical_cells = args.front();
        } else {
            spdlog::info("ignore arg: {}", s);
        }
    }

    if (filler_list) {
        input_cfg.selected_fillers = std::move(split(*filler_list, WHITESPACES));
    }
    if (critical_cells) {
        input_cfg.critical_cells = std::move(split(*critical_cells, WHITESPACES));
    }

    dp::LefDefDumper dumper;
    dumper.run(openroad_);
    dumper.print_dumped_files(std::cout);

    string basename = dumper.basename();

    input_cfg.def = dumper.def();
    string tech = dumper.tech_lef();
    input_cfg.lefs = dumper.lib_lefs();
    input_cfg.lefs.push_front(tech);

    string input_cfg_fn = std::move(write_input_config(input_cfg, basename));
    string outdef = std::move(output_def_name(basename));
    int ret = exec_incr_dp(input_cfg_fn, outdef, baseline, not_stop_when_illegal);
    read_back(outdef);
    return ret;
}

} // namespace linden

