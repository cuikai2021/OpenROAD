#pragma once

namespace ord {
class OpenRoad;
} // namespace ord

namespace odb {
class dbDatabase;
} // namespace odb


namespace utl {
class Logger;
} // namespace utl

namespace lindenmp {

class LindenMacroPlace
{
public:
    explicit LindenMacroPlace(ord::OpenRoad*);
    /// @brief run Linden macro-placement
    /// @return return code.
    //      0 means no error,
    //      1 for unlegalized.
    int run();

    double& mut_density() noexcept {
        return density_;
    }

    double& mut_overflow() noexcept {
        return overflow_;
    }

    bool& mut_use_uniform_density() noexcept {
        return use_uniform_density_;
    }

    size_t& mut_debug_pause_iters_() noexcept {
        return debug_pause_iters_;
    }

    size_t& mut_debug_update_iters_() noexcept {
        return debug_update_iters_;
    }

private:
    odb::dbDatabase* db_;
    utl::Logger* log_;

    double density_;
    double overflow_;
    bool use_uniform_density_;
    size_t debug_pause_iters_;
    size_t debug_update_iters_;
};

} // namespace lindenmp
