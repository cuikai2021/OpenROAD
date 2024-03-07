#include "ffi.hpp"
#include "linden/dp.hpp"
#include "ord/OpenRoad.hh"
#include "fassert.hpp"
#include <spdlog/spdlog.h>
#include <string_view>
#include <cstring>

using namespace std;

namespace linden {

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

DpConfig parse_from(std::deque<std::string> args) {
    string_view FILLER_LIST_ARG("--filler-list");
    DpConfig res;
    optional<string> filler_list = nullopt;
    optional<string> critical_cells = nullopt;

    for(; !args.empty(); args.pop_front()) {
        const auto& s = args.front();
        if (s == FILLER_LIST_ARG) {
            args.pop_front();
            if (args.empty()) {
                spdlog::error(
                    "Required a list of fillers, separated by whitespaces, "
                    "e.g,. \"FILL1 FILL2\".");
            }
            filler_list = args.front();
        } else if (strcmp(s.c_str(), "--not-to-stop-when-illegal") == 0) {
            res.not_stop_when_illegal = true;
        } else if (strcmp(s.c_str(), "--baseline") == 0) {
            args.pop_front();
            if (args.empty()) {
                spdlog::error(
                    "Required a DEF file as the baseline.");
            }
            res.baseline_def = args.front();
        } else if (strcmp(s.c_str(), "--critical-cells") == 0) {
            args.pop_front();
            if (args.empty()) {
                spdlog::error(
                    "Required a list of critical cells, separated by whitespaces, "
                    "e.g,. \"cell0 cell1\".");
            }
            critical_cells = args.front();
        } else if (strcmp(s.c_str(), "--timing-data") == 0) {
            args.pop_front();
            res.timing_data = args.front();
        } else if (strcmp(s.c_str(), "--suppress-output") == 0) {
            // this is a secret arg. it is just for testing so far.
            res.suppress_output = true;
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
    auto cfg = parse_from(args);
    ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
    return basic_dp(ord, cfg);

}

int incr_dp_gogogo(deque<string> args) {
    auto cfg = parse_from(args);
    ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
    return incr_dp(ord, cfg);
}

} // namespace linden
