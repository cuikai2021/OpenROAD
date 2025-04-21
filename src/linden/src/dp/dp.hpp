#pragma once

#include <deque>
#include <filesystem>
#include <string>
#include <string_view>

#include "linden/dp.hpp"

namespace odb {
class dbOrientType;
}

namespace linden::dp {

int exec(const Config& cfg, const std::filesystem::path& exe, const std::deque<std::string>& args, const std::string_view& algorithm);
odb::dbOrientType pb_to_dbOrientType(int pb) noexcept;
std::filesystem::path output_def_name(const std::filesystem::path& basename, bool suppress_output);

} // namespace linden::dp
