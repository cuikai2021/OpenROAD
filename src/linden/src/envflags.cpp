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

