#pragma once

/**
 * This file provides a surprising-proof way to report metrics.
 *
 * So far, OpenROAD reports metrics via spdlog, which is totally wrong.
 * Because logging is for debugging purpose, logs are by-design of large volume and losable.
 * But metrics are parts of features, they must be concise and required.
 *
 * Usage:
 *
 * 1. Configure a sink.
 *    One can choose either stderr or a file.
 *    ```c++
 *    auto cfg = metrics::Config::sink_to_file("path_to_a_file");
 *    // or
 *    // auto cfg = metrics::Config::sink_to_stderr();
 *    ```
 * 2. Constructs a reporter by a configuration.
 *    ```c++
 *    auto reporter = metrics::Reporter(cfg);
 *    ```
 * 3. Reports metrics by string keys.
 *    ```c++
 *    reporter["some_metrics_name"] = 42;
 *    ```
 *    - Metrics values can be anything JSON format can express.
 *      They can be integers, float numbers, booleans, strings, arrays and objects(dicts).
 *    - Dicts can only be expressed by hierarchical indexing, e.g.,
 *      ```c++
 *      reporter["1st_level_key"]["2nd_level_key"] = "str-value";
 *      ```
 *    - Arrays can only be appended, e.g.,
 *      ```c++
 *      reporter["array_example"].append() = 42;
 *      reporter["array_example"].append() = "str";
 *      ```
 *    - Caveat:
 *      + Updates to a same (hierarchical) key are allowed.
 *        The later overwrites the former, i.e., the last one wins.
 * 4. Destroys the reporter object to dump everything to the sink.
 *
 * The complete example is here.
 * ```c++
 * {
 *     auto cfg = MetricsConfig::sink_to_file("metrics.json");
 *     auto reporter = cfg.into_reporter();
 *     reporter["some_metrics_name"] = 42;
 *     reporter["1st_level_key"]["2nd_level_key"] = "str-value";
 *     reporter["array_example"].append() = 42;
 *     reporter["array_example"].append() = "str";
 * }
 * ```
 * After such a code block is executed, `metrics.json` looks like
 * ```json
 * {
 *   "1st_level_key": {
 *     "2nd_level_key": "str-value"
 *   },
 *   "array_example": [
 *     42,
 *     "str"
 *   ],
 *   "some_metrics_name": 42
 * }
 * ```
 */
#include "metrics.ipp"
#include <optional>
#include <filesystem>
#include <map>

namespace metrics {

using Value = impl::Value;

class Config {
public:
    Config(const Config&) =default;
    Config& operator=(const Config&) =default;
    Config(Config&&) =default;
    Config& operator=(Config&&) =default;

    static Config sink_to_file(const std::filesystem::path&) noexcept;
    static Config sink_to_stderr() noexcept;

private:
    Config();

private:
    std::optional<std::filesystem::path> file_path_;

    friend class Reporter;
};

class Reporter {
public:
    Reporter(const Reporter&) =delete;
    Reporter& operator=(const Reporter&) =delete;
    Reporter(Reporter&&) =default;
    Reporter& operator=(Reporter&&) =default;

    explicit Reporter(const Config&) noexcept;
    ~Reporter();

    Value& operator[](const std::string_view& key) noexcept;

private:
    Config config_;
    std::map<std::string, Value> values_;
};

} // namespace metrics
