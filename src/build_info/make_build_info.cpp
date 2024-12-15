extern "C" {
#include <tcl.h>
}


extern "C" {
extern int Build_info_swig_Init(Tcl_Interp* interp);
}

namespace eden {

void init_build_info(Tcl_Interp* interp) {
    Build_info_swig_Init(interp);
}

} // namespace eden
