#pragma once

#include <deque>
#include <string>
#include <filesystem>
#include <optional>

namespace ord {
class OpenRoad;
}

namespace linden {

struct DpConfig
{
    // inputs
    std::optional<std::deque<std::string>> selected_fillers;
    std::optional<std::filesystem::path> timing_data;
    std::optional<std::filesystem::path> routing_congestion_data;
    std::optional<std::string> routing_top_layer;
    // At most one of `critical_cells` and `baseline_def` is allowed to be specified.
    // When both are, the behaviour is quite arbitrary.
    std::optional<std::deque<std::string>> critical_cells;
    std::optional<std::filesystem::path> baseline_def;

    // configurations
    bool suppress_output;
    bool not_stop_when_illegal;

    DpConfig();
};

int basic_dp(::ord::OpenRoad&, const DpConfig&);
int incr_dp(::ord::OpenRoad&, const DpConfig&);
int rddp(::ord::OpenRoad&, DpConfig&);

} // namespace linden
