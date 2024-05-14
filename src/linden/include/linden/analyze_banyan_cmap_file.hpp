#pragma once

#include <filesystem>

namespace linden::cgr {

// analyze banyan's cmap(congestion-map) file and report it in json
bool analyze_banyan_cmap_file(
    const std::filesystem::path& cmap_file,
    const std::filesystem::path& report_file) noexcept;

}  // namespace linden::cgr
