#include "sc/MakeScanChain.h"

#include "sc/ScanChain.h"
#include "db.h"
#include "ord/OpenRoad.hh"
#include "sta/StaMain.hh"

namespace sta {
// Tcl files encoded into strings.
extern const char* sc_tcl_inits[];
}  // namespace sta

extern "C" {
extern int Sc_Init(Tcl_Interp* interp);
}

namespace ord {

sc::ScanChain* makeScanChain() {
  return new sc::ScanChain();
}

void initScanChain(OpenRoad* openroad) {
  Tcl_Interp* tcl_interp = openroad->tclInterp();
  // Define swig TCL commands.
  Sc_Init(tcl_interp);
  sta::evalTclInit(tcl_interp, sta::sc_tcl_inits);
  openroad->getScanChain()->init(openroad);
}

void deleteScanChain(sc::ScanChain* scanChain) {
  delete scanChain;
}

}  // namespace ord
