#include "linden/linden.hpp"
#include "fassert.hpp"
#include "linden/lefdef_dumper.hpp"
#include "toml++/toml.h"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "rsz/Resizer.hh"
#include "sta/Units.hh"
#include "utl/Logger.h"
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

using namespace std;

extern "C" {

int ldp(
    ClientData data,
    Tcl_Interp* interp,
    int objc,
    Tcl_Obj* const argv[]);

void delete_ldp(ClientData data);

} // extern "C"

namespace linden {

const char sTclCmd[] = "linden_detailed_placement";

class DetailedPlacer
{
public:
    explicit DetailedPlacer(::ord::OpenRoad& ord);
    ~DetailedPlacer();

    int go(Tcl_Interp&, deque<reference_wrapper<Tcl_Obj>>&& args);

private:
    ::ord::OpenRoad& openroad_;
};

void initLindenDp(::ord::OpenRoad* ord) {
    Tcl_Interp* interp = ord->tclInterp();
    DetailedPlacer* dp = new DetailedPlacer(*ord);
    Tcl_CreateObjCommand(
        interp,
        sTclCmd,
        ldp,
        reinterpret_cast<ClientData>(dp),
        delete_ldp);
}

void deleteLindenDp(::ord::OpenRoad* ord) {
    Tcl_Interp* interp = ord->tclInterp();
    Tcl_DeleteCommand(interp, sTclCmd);
}

DetailedPlacer::DetailedPlacer(::ord::OpenRoad& ord)
  : openroad_(ord)
{}

DetailedPlacer::~DetailedPlacer() {
}

namespace {

string getExe() {
    const char LINDENDP_EXE[] = "LINDENDP_EXE";
    const char LINDENDP_EXE_DEFAULT[] = "/build/bin/lindendp";
    const char* exe = std::getenv(LINDENDP_EXE);
    if (exe == nullptr) {
        exe = LINDENDP_EXE_DEFAULT;
    }
    return string(exe);
}

string outputDefName(const string& basename) {
    string res(basename);
    res.append(".out.def");
    cout << "out def: " << res << endl;
    return res;
}

int exec(
    utl::Logger& logger,
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
    string exe = std::move(getExe());
    int ec = boost::process::system(exe, boost::process::args(args));
    switch (ec)
    {
    case 0:
        // intend to do nothing
        return ec;
    case 1:
        logger.warn(utl::LDN, 1, "Cannot legalize completely.");
        if (not_stop_when_illegal) {
            logger.info(utl::LDN, 2, "But not to stop the flow.");
            return ec;
        }
        break;
    default:
        break;
    }
    logger.warn(utl::LDN, 2, "Exit code: {}", ec);
    throw std::runtime_error("fail to run lindendp");
}

struct InputArgs
{
    deque<string> lefs;
    string def;
    optional<deque<string>> selected_fillers;
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

  auto log = ::ord::OpenRoad::openRoad()->getLogger();
  log->metric("dp::tns", tns);
  log->metric("dp::wns", wns);
}

} // namespace

int DetailedPlacer::go(
    Tcl_Interp& interp,
    deque<reference_wrapper<Tcl_Obj>>&& args)
{
    InputArgs input_cfg;
    optional<const char*> filler_list = nullopt;
    bool not_stop_when_illegal = false;
    args.pop_front(); // `linden_detailed_placement` itself
    for(; !args.empty(); args.pop_front()) {
        char* s = Tcl_GetString(&args.front().get());
        if (strcmp(s, "--filler-list") == 0) {
            args.pop_front();
            if (args.empty()) {
                openroad_.getLogger()->error(
                    utl::LDN, 0,
                    "Required a list of fillers, separated by whitespaces, "
                    "e.g,. \"FILL1 FILL2\".");
            }
            filler_list = std::move(Tcl_GetString(&args.front().get()));
        } else if (strcmp(s, "--not-to-stop-when-illegal") == 0) {
            not_stop_when_illegal = true;
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
    int ret = exec(*openroad_.getLogger(), input_cfg_fn, outdef, not_stop_when_illegal);
    openroad_.readDef(outdef.c_str(), false, false, true);
    report_metrics();
    return ret;
}

} // namespace linden


extern "C"
int ldp(
    ClientData data,
    Tcl_Interp* interp,
    int objc,
    Tcl_Obj* const argv[])
{
    linden::DetailedPlacer* dp = reinterpret_cast<linden::DetailedPlacer*>(data);
    deque<reference_wrapper<Tcl_Obj>> args;
    for(int i = 0; i < objc; ++i) {
        args.push_back(std::ref(*argv[i]));
    }
    int ret = dp->go(*interp, std::move(args));
    switch (ret) {
        case 0: {
            Tcl_SetResult(interp, "0", TCL_STATIC);
            break;
        }
        case 1: {
            Tcl_SetResult(interp, "1", TCL_STATIC);
            break;
        }
    }
    return TCL_OK;
}

extern "C"
void delete_ldp(ClientData data) {
    linden::DetailedPlacer* dp = reinterpret_cast<linden::DetailedPlacer*>(data);
    delete dp;
}

