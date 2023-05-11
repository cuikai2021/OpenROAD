#include "utl/metrics.hpp"
#include "testa.hpp"
#include "prettyprint.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <limits>

using namespace std;
namespace fs = std::filesystem;

namespace {

string read_all(const fs::path& filename) {
    ostringstream res;
    ifstream fin(filename);
    res << fin.rdbuf();
    return res.str();
}

void compact(string& inout) noexcept {
    string out;
    for(auto c: inout) {
        switch(c) {
            case ' ': case '\n': case '\r': {
                break;
            }
            default: {
                out.push_back(c);
            }
        }
    }
    std::swap(inout, out);
}

} // namespace

namespace {
void int_metrics(const string&)
{
    fs::path filename = fs::path("/tmp/") / __func__;
    auto cfg = metrics::Config::sink_to_file(filename);
    {
        auto reporter = metrics::Reporter(cfg);
        reporter["int_0"] = 42;
        reporter["int_1"] = numeric_limits<int64_t>::max();
    }
    auto trial = read_all(filename);
    compact(trial);
    string expected = "{"
        "\"int_0\":42,"
        "\"int_1\":9223372036854775807"
    "}";
    TESTA_ASSERT(trial == expected)
        (trial)
        (expected)
        .issue();
}
} // namespace
TESTA_DEF_JUNIT_LIKE1(int_metrics);

namespace {
void str_metrics(const string&)
{
    fs::path filename = fs::path("/tmp/") / __func__;
    auto cfg = metrics::Config::sink_to_file(filename);
    {
        auto reporter = metrics::Reporter(cfg);
        reporter["str_0"] = "literal";
        reporter["str_1"] = string("string");
        reporter["str_2"] = string_view("string_view");
    }
    auto trial = read_all(filename);
    compact(trial);
    string expected = "{"
        "\"str_0\":\"literal\","
        "\"str_1\":\"string\","
        "\"str_2\":\"string_view\""
    "}";
    TESTA_ASSERT(trial == expected)
        (trial)
        (expected)
        .issue();
}
} // namespace
TESTA_DEF_JUNIT_LIKE1(str_metrics);

namespace {
void float_metrics(const string&)
{
    fs::path filename = fs::path("/tmp/") / __func__;
    auto cfg = metrics::Config::sink_to_file(filename);
    {
        auto reporter = metrics::Reporter(cfg);
        reporter["float_metrics"] = 0.5;
    }
    auto trial = read_all(filename);
    compact(trial);
    string expected = "{"
        "\"float_metrics\":0.5"
    "}";
    TESTA_ASSERT(trial == expected)
        (trial)
        (expected)
        .issue();
}
} // namespace
TESTA_DEF_JUNIT_LIKE1(float_metrics);

namespace {
void bool_metrics(const string&)
{
    fs::path filename = fs::path("/tmp/") / __func__;
    auto cfg = metrics::Config::sink_to_file(filename);
    {
        auto reporter = metrics::Reporter(cfg);
        reporter["true"] = true;
        reporter["false"] = false;
    }
    auto trial = read_all(filename);
    compact(trial);
    string expected = "{"
        "\"false\":false,"
        "\"true\":true"
    "}";
    TESTA_ASSERT(trial == expected)
        (trial)
        (expected)
        .issue();
}
} // namespace
TESTA_DEF_JUNIT_LIKE1(bool_metrics);

namespace {
void hierachical_metrics(const string&)
{
    fs::path filename = fs::path("/tmp/") / __func__;
    auto cfg = metrics::Config::sink_to_file(filename);
    {
        auto reporter = metrics::Reporter(cfg);
        reporter["1st_level"]["2nd_level"] = 42;
    }
    auto trial = read_all(filename);
    compact(trial);
    string expected = "{"
        "\"1st_level\":{\"2nd_level\":42}"
    "}";
    TESTA_ASSERT(trial == expected)
        (trial)
        (expected)
        .issue();
}
} // namespace
TESTA_DEF_JUNIT_LIKE1(hierachical_metrics);

namespace {
void array_metrics(const string&)
{
    fs::path filename = fs::path("/tmp/") / __func__;
    auto cfg = metrics::Config::sink_to_file(filename);
    {
        auto reporter = metrics::Reporter(cfg);
        reporter["array"].append() = 42;
        reporter["array"].append() = "str";
    }
    auto trial = read_all(filename);
    compact(trial);
    string expected = "{"
        "\"array\":[42,\"str\"]"
    "}";
    TESTA_ASSERT(trial == expected)
        (trial)
        (expected)
        .issue();
}
} // namespace
TESTA_DEF_JUNIT_LIKE1(array_metrics);
