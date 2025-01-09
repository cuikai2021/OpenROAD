#pragma once

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

inline bool is_on_omni_cloud()
{
    const char* pach_in_worker = std::getenv("PACH_IN_WORKER");
    return pach_in_worker && std::string(pach_in_worker) == "true";
}

inline std::optional<std::filesystem::path> omni_tmp_dir()
{
    auto path = std::filesystem::path("/pfs/out/tmp/");
    try {
        if (!std::filesystem::exists(path)) {
            std::filesystem::create_directories(path);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        spdlog::info("Failed to create directory: {}. Error: {}", path.string(), e.what());
        return std::nullopt;
    }
    return path;
}
