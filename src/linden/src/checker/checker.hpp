#pragma once
#include <optional>
#include <string>
namespace ord {
class OpenRoad;
}

namespace linden {
    namespace checker {
        int check_placement(::ord::OpenRoad& ord, bool suppress_output, std::optional<std::string> mode);
    }
};