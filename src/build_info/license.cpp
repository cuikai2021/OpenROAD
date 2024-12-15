#include "license.hpp"
#include "build_info.hpp"
#include <spdlog/spdlog.h>
#include <sstream>
#include <chrono>
#include <cstdlib>
#include <iostream>

using namespace std;

namespace eden {

namespace {

chrono::system_clock::time_point get_build_time() {
    chrono::system_clock::time_point epoch;
    chrono::seconds secs_since_epoch(EDEN_BUILD_TIME_SECS);
    return epoch + secs_since_epoch;
}

} // namespace

void check_license() {
    if (EDEN_TRIAL_PERIOD_DAYS == 0) {
        spdlog::warn("Unlimited days for trial.");
        return;
    }
    auto build_time = get_build_time();
    auto now = chrono::system_clock::now();
    chrono::days timeout(EDEN_TRIAL_PERIOD_DAYS);
    if (now <= build_time + timeout) {
        using float_days = chrono::duration<float, ratio<86400>>;
        float_days remains(build_time + timeout - now);
        spdlog::info(
            "Trial period: {} days, remaining {:.1f} days.",
            EDEN_TRIAL_PERIOD_DAYS,
            remains.count());
    } else {
        spdlog::error("Trial period expired. Exiting...");
        exit(2);
    }
}

} // namespace eden
