#include "checker/ffi.hpp"
#include <spdlog/spdlog.h>
#include <deque>
#include <string>
#include <string_view>
#include "checker.hpp"
#include "ord/OpenRoad.hh"
#include "util/args.hpp"

namespace linden {
    std::optional<std::string> parse_mode_arg(const std::deque<std::string>& args) {
        const std::string_view MODE("--mode");
        Args args_(args);
        for (;;) {
            auto arg = args_.next();
            if (!arg) {
                break;
            }
            const auto& s = *arg;
            if (s == MODE) {
                auto param = args_.next();
                if (!param) {
                    spdlog::error(
                        "{} requires a MODE as the parameter",
                        MODE
                    );
                }
                return *param;
            }
        }
        std::optional<std::string> res;
        return res;
    }
    int check_placement_gogogo(const std::deque<std::string>& args) {
        ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
        std::optional<std::string> mode = parse_mode_arg(args);
        return checker::check_placement(ord, false, mode);
    }
} // namespace linden