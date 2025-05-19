#pragma once

namespace ord {
class OpenRoad;
}

namespace linden {
    namespace checker {
        int check_placement(::ord::OpenRoad& ord, bool suppress_output);
    }
};