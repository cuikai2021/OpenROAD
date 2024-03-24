#include "ffi.hpp"
#include "mp.hpp"
#include "linden/gp.hpp"
#include "../util/args.hpp"
#include <spdlog/spdlog.h>
#include <string_view>
#include <optional>
#include <charconv>

using namespace std;

namespace linden::mp {

namespace {

optional<gp::Config> parse_from(deque<string> args) noexcept {
    const string_view DENSITY("-density");
    const string_view UNIFORM_DENSITY("-uniform_density");
    const string_view OVERFLOW("-overflow");

    gp::Config res(*gp::Config::get_default());
    Args args_(std::move(args));

    for(;;) {
        auto arg = args_.next();
        if (!arg) {
            break;
        }
        auto const& s = *arg;
        if (s == DENSITY) {
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
        } else {
            spdlog::info("ignore arg: {}", s);
        }
    }

    return optional(res);
}

} // namespace

void mp_gogogo(std::deque<std::string> args) noexcept {
    auto cfg = parse_from(std::move(args));
    if (cfg) {
        linden_macro_place(*cfg);
    }
}

} // namespace linden::mp

