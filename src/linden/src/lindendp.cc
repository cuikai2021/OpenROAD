#include "linden/linden.hpp"
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
#include <tcl.h>
#include <cstdlib>

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

const char sTclCmd[] = "lindendp";

class DetailedPlacer
{
public:
    explicit DetailedPlacer(::ord::OpenRoad& ord);
    ~DetailedPlacer();

    void go();

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
    const string& tech,
    const deque<string>& libs,
    const string& indef,
    const string& outdef)
{
    deque<string> args;
    args.push_back("--cell-only");
    args.push_back("--lef");
    args.push_back(tech);
    for(const auto& x: libs) {
        args.push_back(x);
    }
    args.push_back("--def");
    args.push_back(indef);
    args.push_back("--to");
    args.push_back(outdef);
    string exe = std::move(getExe());
    int ec = boost::process::system(exe, boost::process::args(args));
    if (ec != 0) {
        cout << "Exit code: " << ec << endl;
        throw std::runtime_error("fail to run lindendp");
    }
}

} // namespace

void DetailedPlacer::go() {
    string basename = std::move(genBaseFilename());

    string indef = std::move(writeDef(openroad_, basename));
    string tech = std::move(writeTech(openroad_, basename));
    deque<string> libs = std::move(writeLib(openroad_, basename));
    string outdef = std::move(outputDefName(basename));
    exec(tech, libs, indef, outdef);
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
    dp->go();
    return 0;
}

extern "C"
void delete_ldp(ClientData data) {
    linden::DetailedPlacer* dp = reinterpret_cast<linden::DetailedPlacer*>(data);
    delete dp;
}

