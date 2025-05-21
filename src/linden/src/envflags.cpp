#include "envflag/envflag.hpp"

DEFINE_F64_FLAG_WITH_DEFAULT(LINDEN_IGNORED_NETS_MAX_FANOUT,
    "threshold of the max-fanout number to determine whether a net is ignored.",
    200)

DEFINE_F64_FLAG_WITH_DEFAULT(LINDEN_SCAN_ENABLE_WEIGHT,
    "net weight for those connecting to scan enables.",
    0)
DEFINE_F64_FLAG_WITH_DEFAULT(LINDEN_CLOCK_WEIGHT,
    "net weight for those connecting to clock pins.",
    0)
DEFINE_F64_FLAG_WITH_DEFAULT(LINDEN_RESET_WEIGHT,
    "net weight for those connecting to reset pins.",
    1)

DEFINE_STR_FLAG(LINDEN_INITIAL_PLACEMENT_RESULT_IN_DEF_FILE,
    "debug: save linden's initial placement result in def. empty means no dump"
)

DEFINE_STR_FLAG_WITH_DEFAULT(LINDEN_TMP_FILE_ROOT_DIR,
    "Debug: specify the root directory to dump the internal files in linden flow",
    "/tmp/"
)

DEFINE_STR_FLAG(LINDEN_GP_METRICS_FILE,
    "Where to dump metrics after Linden global placement. Default is stderr."
)
DEFINE_STR_FLAG(LINDEN_MP_METRICS_FILE,
    "Where to dump metrics after Linden macro placement. Default is stderr."
)
DEFINE_BOOL_FLAG_WITH_DEFAULT(LINDEN_GP_CELL_RELATIVE_POS_CAL,
    "Enable the calculation of change in cell relative position",
    false
)

DEFINE_STR_FLAG_WITH_DEFAULT(LINDENDP_EXE_DIR,
    "Debug: specify the root directory to place `lindendp` and `linden-incremental-legalizer`",
    "/build/bin/"
)

DEFINE_STR_FLAG_WITH_DEFAULT(
    LINDEN_DP_ALGORITHM,
    "Specify the algorithm chosen for placement",
    "deblais"
)

// flags for debugging purpose
DEFINE_BOOL_FLAG_WITH_DEFAULT(LINDEN_IN_REGRESSION,
    "Disable timestamp in spdlog, when run in regression mode",
    false
)
DEFINE_I64_FLAG_WITH_DEFAULT(LINDEN_GUI_REFRESH,
    "How many iterations for GP to refresh GUI",
    10
)
DEFINE_I64_FLAG_WITH_DEFAULT(LINDEN_GUI_PAUSE,
    "How many iterations for GP to pause",
    100
)

DEFINE_STR_FLAG(LINDEN_IP_DEBUG_DIR,
    "The dir to store files for debugging initial placement"
);
DEFINE_I64_FLAG_WITH_DEFAULT(LINDEN_GP_CMAP_PERIOD,
    "The routability-driven GP will estimate congestion maps periodically. "
    "This flag controls the period (in iterations).",
    10
);

DEFINE_I64_FLAG_WITH_DEFAULT(LINDEN_GP_CMAP_ITERATION,
    "The routability-driven GP will estimate congestion maps periodically. "
    "This flag controls the period .",
    100
);
