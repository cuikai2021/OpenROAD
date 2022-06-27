#include <tcl.h>
#include "sta/StaMain.hh"
#include "ord/OpenRoad.hh"
#include "gpl/MakeLindenGlobalPlace.h"
#include "gpl/lindengp.h"

namespace sta {
extern const char *gpl_tcl_inits[];
}

extern "C" {
extern int Gpl_Init(Tcl_Interp* interp);
}

namespace ord {

gpl::LindenGlobalPlace* 
makeLindenGlobalPlace() {
  return new gpl::LindenGlobalPlace();
}

void
initLindenGlobalPlace(OpenRoad* openroad) {
  Tcl_Interp* tcl_interp = openroad->tclInterp();
  Gpl_Init(tcl_interp);
  sta::evalTclInit(tcl_interp, sta::gpl_tcl_inits);
  openroad->getLindenGlobalPlace()->init(openroad->getDb(), openroad->getLogger());
}

void
deleteLindenGlobalPlace(gpl::LindenGlobalPlace *gp) {
  delete gp;
}

}
