#include "envflag/envflag.hpp"

DEFINE_BOOL_FLAG_WITH_DEFAULT(RSZ_DISABLE_BUFFERING,
    "option flag to disable buffering while repair_timing",
    false
)

DEFINE_BOOL_FLAG_WITH_DEFAULT(RSZ_DISABLE_RC_MODIFICATION,
    "option flag to disable rc modification while repair_timing",
    false
)
