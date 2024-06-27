#include "sta/StaMain.hh"
#include "ord/OpenRoad.hh"

extern "C" {
#include <tcl.h>
}


namespace sta {
extern const char *linden_tcl_inits[];
}

extern "C" {
extern int Linden_Init(Tcl_Interp* interp);
}

namespace linden {

void init(::ord::OpenRoad* ord) {
    Tcl_Interp* interp = ord->tclInterp();
    Linden_Init(interp);
    sta::evalTclInit(interp, sta::linden_tcl_inits);
}

} // namespace linden
