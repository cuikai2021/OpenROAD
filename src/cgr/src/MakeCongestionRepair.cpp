#include <tcl.h>
#include "sta/StaMain.hh"
#include "ord/OpenRoad.hh"
#include "cgr/MakeCongestionRepair.hpp"
#include "cgr/congestionRepair.hpp"

namespace sta {
extern const char *cgr_tcl_inits[];
}

extern "C" {
extern int Cgr_Init(Tcl_Interp* interp);
}

namespace ord {

cgr::CongestionRepair* 
makeCongestionRepair() {
  return new cgr::CongestionRepair();
}

void
initCongestionRepair(OpenRoad* openroad) {
  Tcl_Interp* tcl_interp = openroad->tclInterp();
  Cgr_Init(tcl_interp);
  sta::evalTclInit(tcl_interp, sta::cgr_tcl_inits);
  openroad->getCongestionRepair()->init(openroad->getDb(), openroad->getLogger());
}

void
deleteCongestionRepair(cgr::CongestionRepair *gp) {
  delete gp;
}

}
