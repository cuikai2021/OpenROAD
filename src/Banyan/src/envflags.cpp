#include "envflag/envflag.hpp"

DEFINE_BOOL_FLAG_WITH_DEFAULT(BANYAN_SUPPRESS_EXE_LOG,
    "Suppress logs from banyan exe",
    false
)

DEFINE_STR_FLAG_WITH_DEFAULT(DEBLAIS_ROOT_DIR,
    "Debug: specify the root directory to dump the internal files in deblais",
    "/tmp/deblais"
)