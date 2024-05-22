#pragma once

#include <deque>
#include <optional>
#include <filesystem>
#include <ostream>

namespace ord {
class OpenRoad;
}

namespace linden {

// dump lef/def files to /tmp directory for linden::dp to use
struct LefDefDumper
{
    std::filesystem::path basename;
    std::filesystem::path tech_lef;
    std::deque<std::filesystem::path> lib_lefs;
    std::filesystem::path def;

    explicit LefDefDumper(::ord::OpenRoad& ord);
    std::ostream& print_dumped_files(std::ostream& oss) const;
};

std::filesystem::path gen_base_filename();

}  // namespace linden
