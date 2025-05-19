#include "checker/ffi.hpp"
#include "checker.hpp"
#include "ord/OpenRoad.hh"
namespace linden {
    int check_placement_gogogo() {
        ord::OpenRoad& ord = *ord::OpenRoad::openRoad();
        return checker::check_placement(ord, false);
    }
} // namespace linden