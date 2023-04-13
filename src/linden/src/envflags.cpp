#include "envflag/envflag.hpp"


DEFINE_F64_FLAG_WITH_DEFAULT(LINDEN_SCAN_ENABLE_WEIGHT,
    "net weight for those connecting to scan enables.",
    0);
DEFINE_F64_FLAG_WITH_DEFAULT(LINDEN_CLOCK_WEIGHT,
    "net weight for those connecting to clock pins.",
    0);
DEFINE_F64_FLAG_WITH_DEFAULT(LINDEN_RESET_WEIGHT,
    "net weight for those connecting to reset pins.",
    0);

DEFINE_BOOL_FLAG_WITH_DEFAULT(LINDEN_CGR_DUMP_DEF_BEFORE_CALL_BABY_BANYAN,
    "debug flag for dumping def file for debugging baby-banyan in CGR run",
    false
)

DEFINE_STR_FLAG_WITH_DEFAULT(LINDEN_CGR_CMAP_DEBUGGING_FILE,
    "debug: loading cmap from disk without running baby-banyan",
    ""
)

DEFINE_STR_FLAG_WITH_DEFAULT(LINDEN_TMP_FILE_ROOT_DIR,
    "Debug: specify the root directory to dump the internal files in linden flow",
    "/tmp/"
)

