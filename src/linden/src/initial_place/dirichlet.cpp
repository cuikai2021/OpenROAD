#include "dirichlet.hpp"
#include "../util/odb_util.hpp"
#include "ord/OpenRoad.hh"
#include "fassert.hpp"
#include <spdlog/spdlog.h>
#include <string>

extern "C" {
#include <tcl.h>
}

using namespace std;

namespace linden::ip::dirichlet {

namespace {

void sync_cell_positions_to_ldb(linden::db::Database& ldb) {
    for (auto& x: ldb.mut_moveable_insts()) {
        auto& cell = x.get();
        odb::dbInst* oinst = reinterpret_cast<odb::dbInst*>(cell.odb_inst());
        if (oinst == nullptr) {
            continue;
        }
        FASSERT(cell.moveable())
            (cell.name());
        linden::geom::Rect shape = inst_located_shape(*oinst);
        cell.move_to(shape.c());
    }
}

} // namespace

void run(const linden::gp::Config& cfg, linden::db::Database& ldb) {
    const char CMD[] = "gpl::replace_initial_place_cmd\n"
        "gpl::replace_reset_cmd"; // reset `replace` to release memory
    ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
    Tcl_Interp* tcl = ord.tclInterp();
    Tcl_Eval(tcl, CMD);
    sync_cell_positions_to_ldb(ldb);
}

} // namespace linden::ip::dirichlet
