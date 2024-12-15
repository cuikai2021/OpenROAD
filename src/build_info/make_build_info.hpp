#pragma once

extern "C" {
struct Tcl_Interp;
}

namespace eden {
void init_build_info(Tcl_Interp*);
} // namespace eden
