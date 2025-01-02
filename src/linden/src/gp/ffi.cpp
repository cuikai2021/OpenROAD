#include "ffi.hpp"
#include "../util/args.hpp"
#include "linden/gp.hpp"
#include <spdlog/spdlog.h>
#include <string_view>
#include <optional>
#include <charconv>

using namespace std;

namespace linden::gp {

namespace {

optional<Config> parse_from(deque<string> args) noexcept {
    const string_view INITIAL_PLACE_POLICY("-initial_place_policy");
    const string_view DENSITY("-density");
    const string_view UNIFORM_DENSITY("-uniform_density");
    const string_view OVERFLOW("-overflow");
    const string_view TIMING_DRIVEN("-timing_driven");
    const string_view CONTINUE("-continue");
    const string_view ROUTABILITY_DRIVEN("-routability_driven");
    const string_view OVERFLOW_TO_EST_CMAP("-overflow_to_estimate_cmap");

    Config res(*Config::get_default());
    Args args_(std::move(args));

    for(;;) {
        auto arg = args_.next();
        if (!arg) {
            break;
        }
        auto const& s = *arg;
        if (s == INITIAL_PLACE_POLICY) {
            auto param = args_.next();
            if (!param) {
                spdlog::error(
                    "{} requires a parameter, such as def or dirichlet.",
                    INITIAL_PLACE_POLICY);
                return nullopt;
            }
            res.initial_place_policy = Config::InitialPlacePolicy::from_str(*param);
        } else if (s == DENSITY) {
            auto param = args_.next();
            if (!param) {
                spdlog::error(
                    "{} requires a numeric parameter.",
                    DENSITY);
                return nullopt;
            }
            float x = 0;
            auto [ptr, ec] = from_chars(param->data(), param->data() + param->size(), x);
            if (ec != std::errc() || ptr < param->data() + param->size()) {
                spdlog::error(
                    "{} requires a numeric parameter.",
                    DENSITY);
                return nullopt;
            }
            if (x < 0 || x > 1) {
                spdlog::error(
                    "{} must be between 0 and 1.",
                    DENSITY);
                return nullopt;
            }
            res.mut_density() = x;
        } else if (s == UNIFORM_DENSITY) {
            res.set_to_uniform();
        } else if (s == OVERFLOW) {
            auto param = args_.next();
            if (!param) {
                spdlog::error(
                    "{} requires a numeric parameter.",
                    OVERFLOW);
                return nullopt;
            }
            float x = 0;
            auto [ptr, ec] = from_chars(param->data(), param->data() + param->size(), x);
            if (ec != std::errc() || ptr < param->data() + param->size()) {
                spdlog::error(
                    "{} requires a numeric parameter.",
                    OVERFLOW);
                return nullopt;
            }
            if (x < 0 || x > 1) {
                spdlog::error(
                    "{} must be between 0 and 1.",
                    OVERFLOW);
                return nullopt;
            }
            res.overflow = x;
        } else if (s == TIMING_DRIVEN) {
            res.timing_driven = true;
        } else if (s == CONTINUE) {
            res.continue_mode = true;
        } else if (s == ROUTABILITY_DRIVEN) {
            res.routability_driven = true;
        } else if (s == OVERFLOW_TO_EST_CMAP) {
            auto param = args_.next();
            if (!param) {
                spdlog::error(
                    "{} requires a numeric parameter.",
                    OVERFLOW_TO_EST_CMAP);
                return nullopt;
            }
            float x = 0;
            auto [ptr, ec] = from_chars(param->data(), param->data() + param->size(), x);
            if (ec != std::errc() || ptr < param->data() + param->size()) {
                spdlog::error(
                    "{} requires a numeric parameter.",
                    OVERFLOW_TO_EST_CMAP);
                return nullopt;
            }
            if (x < 0 || x > 1) {
                spdlog::error(
                    "{} must be between 0 and 1.",
                    OVERFLOW_TO_EST_CMAP);
                return nullopt;
            }
            res.overflow_to_estimate_cmap = x;
        } else {
            spdlog::info("ignore arg: {}", s);
        }
    }

    return optional(res);
}

} // namespace

void gogogo(deque<string> args) {
    auto cfg = parse_from(std::move(args));
    if (cfg) {
        linden_global_place(*cfg);
    }
}

} // namespace linden::gp
