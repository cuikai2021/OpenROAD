#include "linden/lindendp.hpp"
#include "fassert.hpp"
#include "linden/lefdef_dumper.hpp"
#include "toml++/toml.h"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "rsz/Resizer.hh"
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
IMPORT_STR_FLAG_WITH_DEFAULT(LINDENDP_EXE);

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

std::filesystem::path getExe() {
    return std::filesystem::path(LINDENDP_EXE());
}

string outputDefName(const string& basename) {
    string res(basename);
    res.append(".out.def");
    cout << "out def: " << res << endl;
    return res;
}

int exec(
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
    auto exe = std::move(getExe());
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

struct InputArgs
{
    deque<string> lefs;
    string def;
    optional<deque<string>> selected_fillers;
    optional<string> timing_data;
};

string writeInputConfig(
    const InputArgs& args,
    const string& basename)
{
    auto root = toml::table{
        {"version", "1.0"},
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

    string filename = basename;
    filename.append(".toml");
    ofstream fout(filename);
    fout << root;
    fout.close();
    cout << "input-cfg: " << filename << endl;
    return filename;
}

void skip_whitespaces(string_view& str, const string& whitespace) {
    size_t n = str.find_first_not_of(whitespace);
    if (n == string_view::npos) {
        str = string_view();
    } else {
        str = str.substr(n);
    }
}

string_view nonwhitespaces(const string_view& str, const string& whitespace) {
    size_t n = str.find_first_of(whitespace);
    if (n == string_view::npos) {
        return str;
    } else {
        return str.substr(0, n);
    }
}

void report_metrics() {
  rsz::Resizer* rs = ::ord::OpenRoad::openRoad()->getResizer();
  rs->estimateParasitics(rsz::ParasiticsSrc::placement);
  auto sta = ::ord::OpenRoad::openRoad()->getSta();

  const int DIGITS = 2;
  auto total_negative_slack = sta->totalNegativeSlack(sta::MinMax::max());

  sta::Slack worst_slack;
  sta::Vertex* worst_vertex;
  sta->worstSlack(sta::MinMax::max(), worst_slack, worst_vertex);

  auto tns = sta->units()->timeUnit()->asString(total_negative_slack, DIGITS);
  auto wns = sta->units()->timeUnit()->asString(worst_slack, DIGITS);

  metrics::Config mcfg = metrics::Config::sink_to_stderr();
  if(auto opt = LINDEN_DP_METRICS_FILE(); opt) {
    mcfg = metrics::Config::sink_to_file(*opt);
  }
  metrics::Reporter reporter(mcfg);
  // TODO:
  // 1. shall we convert tns and wns to numbers?
  // 2. shall metrics hierarchically indexed?
  reporter["dp"]["tns"] = tns;
  reporter["dp"]["wns"] = wns;
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
        deque<string> selected_fillers;
        string_view lst(*filler_list);
        string whitespace(" \t\n");
        for(;; ) {
            skip_whitespaces(lst, whitespace);
            if (lst.empty()) {
                break;
            }
            auto const& filler = nonwhitespaces(lst, whitespace);
            FASSERT(!filler.empty())
                (lst);
            selected_fillers.push_back(string(filler));
            lst = lst.substr(filler.length());
        }
        input_cfg.selected_fillers = std::move(selected_fillers);
    }

    dp::LefDefDumper dumper;
    dumper.run(openroad_);
    dumper.print_dumped_files(std::cout);

    string basename = dumper.basename();

    input_cfg.def = dumper.def();
    string tech = dumper.tech_lef();
    input_cfg.lefs = dumper.lib_lefs();
    input_cfg.lefs.push_front(tech);

    string input_cfg_fn = std::move(writeInputConfig(input_cfg, basename));
    string outdef = std::move(outputDefName(basename));
    int ret = exec(input_cfg_fn, outdef, not_stop_when_illegal);
    openroad_.readDef(outdef.c_str(), false, false, true);
    report_metrics();
    return ret;
}

int IncrLegalizer::go(deque<string>& args)
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
        } else {
            spdlog::info("ignore arg: {}", s);
        }
    }

    if (filler_list) {
        deque<string> selected_fillers;
        string_view lst(*filler_list);
        string whitespace(" \t\n");
        for(;; ) {
            skip_whitespaces(lst, whitespace);
            if (lst.empty()) {
                break;
            }
            auto const& filler = nonwhitespaces(lst, whitespace);
            FASSERT(!filler.empty())
                (lst);
            selected_fillers.push_back(string(filler));
            lst = lst.substr(filler.length());
        }
        input_cfg.selected_fillers = std::move(selected_fillers);
    }

    dp::LefDefDumper dumper;
    dumper.run(openroad_);
    dumper.print_dumped_files(std::cout);

    string basename = dumper.basename();

    input_cfg.def = dumper.def();
    string tech = dumper.tech_lef();
    input_cfg.lefs = dumper.lib_lefs();
    input_cfg.lefs.push_front(tech);

    string input_cfg_fn = std::move(writeInputConfig(input_cfg, basename));
    string outdef = std::move(outputDefName(basename));
    int ret = exec(input_cfg_fn, outdef, not_stop_when_illegal);
    openroad_.readDef(outdef.c_str(), false, false, true);
    report_metrics();
    return ret;
}

} // namespace linden

