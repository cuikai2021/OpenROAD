#include "ffi.hpp"
#include "../util/args.hpp"
#include "linden/dp.hpp"
#include "ord/OpenRoad.hh"
#include "fassert.hpp"
#include <spdlog/spdlog.h>
#include <string_view>
#include <cstring>
#include <charconv>

using namespace std;

namespace linden::dp {

namespace {

char const WHITESPACES[] = " \t\n";

void skip_whitespaces(string_view& str, const string_view& whitespace) noexcept {
    size_t n = str.find_first_not_of(whitespace);
    if (n == string_view::npos) {
        str = string_view();
    } else {
        str = str.substr(n);
    }
}

string_view nonwhitespaces(const string_view& str, const string_view& whitespace) noexcept {
    size_t n = str.find_first_of(whitespace);
    if (n == string_view::npos) {
        return str;
    } else {
        return str.substr(0, n);
    }
}

deque<string> split(const string& raw, const string_view& seps) noexcept {
    deque<string> res;
    string_view view(raw);
    for(;;) {
        skip_whitespaces(view, seps);
        if (view.empty()) {
            break;
        }
        auto const& r = nonwhitespaces(view, seps);
        FASSERT(!r.empty())
            (view);
        res.push_back(string(r));
        view = view.substr(r.length());
    }
    return res;
}


optional<Config> parse_from(std::deque<std::string> args) {
    const string_view FILLER_LIST_ARG("--filler-list");
    const string_view NOT_TO_STOP_WHEN_ILLEGAL("--not-to-stop-when-illegal");
    const string_view BASELINE("--baseline");
    const string_view CRITICAL_CELLS("--critical-cells");
    const string_view TIMING_DATA("--timing-data");
    const string_view SUPPRESS_OUTPUT("--suppress-output");
    const string_view ROUTING_TOP_LAYER("--routing-top-layer");
    const string_view EXTRA_PG_SPACING("--extra-pg-spacing");
    const string_view DISABLE_UPDATING_PARASITICS_AFTER_LEGALIZING("--disable-updating-parasitics-after-legalizing");

    Config res;
    optional<string> filler_list = nullopt;
    optional<string> critical_cells = nullopt;
    Args args_(args);

    for(;;) {
        auto arg = args_.next();
        if (!arg) {
            break;
        }
        const auto& s = *arg;
        if (s == FILLER_LIST_ARG) {
            auto param = args_.next();
            if (!param) {
                spdlog::error(
                    "{} requires a list of fillers, separated by whitespaces, "
                    "e.g,. \"FILL1 FILL2\".",
                    FILLER_LIST_ARG);
                return nullopt;
            }
            filler_list = *param;
        } else if (s == NOT_TO_STOP_WHEN_ILLEGAL) {
            res.not_stop_when_illegal = true;
        } else if (s == BASELINE) {
            auto param = args_.next();
            if (!param) {
                spdlog::error(
                    "{} requires a DEF file as the parameter.",
                    BASELINE);
                return nullopt;
            }
            res.baseline_def = *param;
        } else if (s == CRITICAL_CELLS) {
            auto param = args_.next();
            if (!param) {
                spdlog::error(
                    "{} requires a list of critical cells, separated by whitespaces, "
                    "e.g,. \"cell0 cell1\".",
                    CRITICAL_CELLS);
                return nullopt;
            }
            critical_cells = *param;
        } else if (s == TIMING_DATA) {
            auto param = args_.next();
            if (!param) {
                spdlog::error(
                    "{} requires a timing-data file as its parameter.",
                    TIMING_DATA);
                return nullopt;
            }
            res.timing_data = *param;
        } else if (s == SUPPRESS_OUTPUT) {
            // this is a secret arg. it is just for testing so far.
            res.suppress_output = true;
        } else if (s == ROUTING_TOP_LAYER) {
            auto param = args_.next();
            if (!param) {
                spdlog::error(
                    "{} requires a top layer to tell banyan what layers are routable.",
                    ROUTING_TOP_LAYER
                );
                return nullopt;
            }
            res.routing_top_layer = *param;
        } else if (s == EXTRA_PG_SPACING) {
            auto layer = args_.next();
            if (!layer) {
                spdlog::error("{} requires a layer.", EXTRA_PG_SPACING);
                return nullopt;
            }
            auto spacing_s = args_.next();
            if (!spacing_s) {
                spdlog::error("{} requires spacing.", EXTRA_PG_SPACING);
                return nullopt;
            }
            double spacing = 0;
            auto [ptr, ec] = from_chars(
                spacing_s->data(), spacing_s->data() + spacing_s->size(),
                spacing);
            if (ec != std::errc() || ptr < spacing_s->data() + spacing_s->size()) {
                spdlog::error(
                    "{} requires a numeric parameter.",
                    EXTRA_PG_SPACING);
                return nullopt;
            }
            if (spacing < 0) {
                spdlog::error(
                    "The spacing in {} must be nonnegative.",
                    EXTRA_PG_SPACING);
                return nullopt;
            }
            res.extra_pg_spacing[*layer] = spacing;
        } else if (s == DISABLE_UPDATING_PARASITICS_AFTER_LEGALIZING) {
            res.disable_updating_parasitics = true;
        } else {
            spdlog::info("ignore arg: {}", s);
        }
    }

    if (filler_list) {
        res.selected_fillers = std::move(split(*filler_list, WHITESPACES));
    }
    if (critical_cells) {
        res.critical_cells = std::move(split(*critical_cells, WHITESPACES));
    }

    return res;
}

} // namespace


int basic_dp_gogogo(deque<string> args) {
    auto local_cfg = parse_from(args);
    if (local_cfg) {
        ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
        auto cfg = *Config::get_default();
        cfg += *local_cfg;
        return basic_dp(ord, cfg);
    } else {
        return 50;
    }
}

int incr_dp_gogogo(deque<string> args) {
    auto local_cfg = parse_from(args);
    if (local_cfg) {
        ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
        auto cfg = *Config::get_default();
        cfg += *local_cfg;
        return incr_dp(ord, cfg);
    } else {
        return 50;
    }
}

int rddp_gogogo(deque<string> args) {
    auto local_cfg = parse_from(args);
    if (local_cfg) {
        ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
        auto cfg = *Config::get_default();
        cfg += *local_cfg;
        return rddp(ord, cfg);
    } else {
        return 50;
    }
}

int opt_gogogo(deque<string> args) {
    auto local_cfg = parse_from(args);
    if (local_cfg) {
        ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
        auto cfg = *Config::get_default();
        cfg += *local_cfg;
        return opt(ord, cfg);
    } else {
        return 50;
    }
}

int default_config_gogogo(std::deque<std::string> args) {
    auto new_cfg = parse_from(args);
    if (new_cfg) {
        *Config::get_default() += *new_cfg;
        return 0;
    } else {
        return 50;
    }
}

} // namespace linden::dp
