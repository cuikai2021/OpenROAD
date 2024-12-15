#pragma once
#include <cstdint>

#if __cplusplus
extern "C" {
#endif

extern char const * const EDEN_COMMIT_ID;
extern char const * const EDEN_BUILD_TIME;
extern uint32_t EDEN_TRIAL_PERIOD_DAYS;
extern int64_t EDEN_BUILD_TIME_SECS;

#if __cplusplus
} // extern "C"
#endif
