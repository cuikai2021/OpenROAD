#pragma once

#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <map>
#include <memory>

namespace ord {
class OpenRoad;
}
namespace odb {
  class dbOrientType;
}

namespace linden::dp {

struct Config
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
    std::optional<bool> suppress_output;
    std::optional<bool> not_stop_when_illegal;
    // layer name -> extra spacing for P/G (in nm)
    std::map<std::string, double> extra_pg_spacing;
    
    // sometimes, it is better not to update parasitics after legalizing
    // too see more details: https://z36j6ypbg0.feishu.cn/docx/Ncb9dyRl8o9cIaxpeSWcGYorn3g
    std::optional<bool> disable_updating_parasitics;

    // banyan need this to check pin-access
    std::optional<std::string> min_layer;
    std::optional<std::string> max_layer;

    Config();

    static std::shared_ptr<Config> get_default() noexcept;

    Config& operator+=(const Config&);

    void prettyPrint(std::string&) const;
};

int basic_dp(::ord::OpenRoad&, const Config&);
int incr_dp(::ord::OpenRoad&, const Config&);
int rddp(::ord::OpenRoad&, Config&);
int opt(::ord::OpenRoad&,const Config&);

} // namespace linden::dp
