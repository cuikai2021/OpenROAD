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

} // namespace
// namespace ord {

// gpl::LindenGlobalPlace*
// makeLindenGlobalPlace() {
//   return new gpl::LindenGlobalPlace();
// }

// void
// initLindenGlobalPlace(OpenRoad* openroad) {
//   Tcl_Interp* tcl_interp = openroad->tclInterp();
//   Linden_Init(tcl_interp);
//   sta::evalTclInit(tcl_interp, sta::linden_tcl_inits);
// //   FIXME:
// //   openroad->getLindenGlobalPlace()->init(openroad->getDb(), openroad->getLogger());
// }

// void
// deleteLindenGlobalPlace(gpl::LindenGlobalPlace *gp) {
//   delete gp;
// }

// }
