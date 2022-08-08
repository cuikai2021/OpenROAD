#include "linden/linden.hpp"
#include "fassert.hpp"
#include "toml++/toml.h"
#include "odb/db.h"
#include "odb/lefout.h"
#include "ord/OpenRoad.hh"
#include <boost/process.hpp>
#include <exception>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
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

    void go(Tcl_Interp&, deque<reference_wrapper<Tcl_Obj>>&& args);

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

string genBaseFilename() {
    auto tm = chrono::system_clock::now().time_since_epoch();
    auto tm_ms = chrono::duration_cast<chrono::milliseconds>(tm).count();
    ostringstream oss;
    oss << "/tmp/lindendp_" << tm_ms;
    return oss.str();
}

string writeTech(
    ::ord::OpenRoad& openroad,
    const string& basename)
{
    string filename = basename;
    filename.append(".tech.lef");
    ::odb::lefout out(openroad.getLogger());
    out.writeTech(openroad.getDb()->getTech(), filename.c_str());
    cout << "tech: " << filename << endl;
    return filename;
}

deque<string> writeLib(
    ::ord::OpenRoad& openroad,
    const string& basename)
{
    const auto& libs = openroad.getDb()->getLibs();
    deque<string> res;
    for(const auto& lib: libs) {
        string filename = basename;
        filename.push_back('.');
        filename.append(lib->getName());
        filename.append(".lib.lef");
        ::odb::lefout out(openroad.getLogger());
        out.writeLib(const_cast<::odb::dbLib*>(lib), filename.c_str());
        cout << "lib: " << filename << endl;
        res.push_back(std::move(filename));
    }
    return res;
}

string writeDef(
    ::ord::OpenRoad& openroad,
    const string& basename)
{
    string filename = basename;
    filename.append(".def");
    openroad.writeDef(filename.c_str(), "5.8");
    cout << "in def: " << filename << endl;
    return filename;
}

string getExe() {
    const char LINDENDP_EXE[] = "LINDENDP_EXE";
    const char LINDENDP_EXE_DEFAULT[] = "/usr/local/bin/lindendp";
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

void exec(
    const string& input_cfg,
    const string& outdef)
{
    deque<string> args;
    args.push_back("--input-config");
    args.push_back(input_cfg);
    args.push_back("--to");
    args.push_back(outdef);
    args.push_back("--cell-only");
    string exe = std::move(getExe());
    int ec = boost::process::system(exe, boost::process::args(args));
    if (ec != 0) {
        cout << "Exit code: " << ec << endl;
        throw std::runtime_error("fail to run lindendp");
    }
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

} // namespace

void DetailedPlacer::go(
    Tcl_Interp& interp,
    deque<reference_wrapper<Tcl_Obj>>&& args)
{
    InputArgs input_cfg;
    optional<const char*> filler_list = nullopt;
    args.pop_front(); // `linden_detailed_placement` itself
    for(; !args.empty(); ) {
        char* s = Tcl_GetString(&args.front().get());
        if (strcmp(s, "--filler-list") == 0) {
            args.pop_front();
            if (args.empty()) {
                cout << "Required a list of fillers, separated by whitespaces, e.g,. \"FILL1 FILL2\"." << endl;
                throw std::runtime_error("Required a list of fillers.");
            }
            filler_list = std::move(Tcl_GetString(&args.front().get()));
            args.pop_front();
        } else {
            args.pop_front();
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

    string basename = std::move(genBaseFilename());

    input_cfg.def = std::move(writeDef(openroad_, basename));
    string tech = std::move(writeTech(openroad_, basename));
    input_cfg.lefs = std::move(writeLib(openroad_, basename));
    input_cfg.lefs.push_front(tech);
    string input_cfg_fn = std::move(writeInputConfig(input_cfg, basename));
    string outdef = std::move(outputDefName(basename));
    exec(input_cfg_fn, outdef);
    openroad_.readDef(outdef.c_str(), false, false, true);
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
    dp->go(*interp, std::move(args));
    return 0;
}

extern "C"
void delete_ldp(ClientData data) {
    linden::DetailedPlacer* dp = reinterpret_cast<linden::DetailedPlacer*>(data);
    delete dp;
}

