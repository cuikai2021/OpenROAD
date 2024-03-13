#include "linden/gp.hpp"
#include "fassert.hpp"

using namespace std;

namespace linden {
namespace gp {

const float DEFAULT_DENSITY = 1.0;
const float DEFAULT_OVERFLOW = 0.1;

Config::Config()
:   density(DEFAULT_DENSITY),
    overflow(DEFAULT_OVERFLOW)
{}

shared_ptr<Config> Config::get_default() noexcept {
    static shared_ptr<Config> cfg(new Config());
    return cfg;
}

OptionalSetter<float> Config::mut_density() noexcept {
    float d = density.value_or(DEFAULT_DENSITY);
    return OptionalSetter<float>(density, d);
}

namespace {

const char CSTR_DEF[] = "def";
const char CSTR_DIRICHLET[] = "dirichlet";

} // namespace

string_view Config::InitialPlacePolicy::to_str_view() const noexcept {
    switch (value) {
    case DEF:
        return string_view(CSTR_DEF, sizeof(CSTR_DEF) / sizeof(CSTR_DEF[0]) - 1);
    case DIRICHLET:
        return string_view(CSTR_DIRICHLET, sizeof(CSTR_DIRICHLET) / sizeof(CSTR_DIRICHLET[0]) - 1);
    }
    FASSERT(false);
    return string_view();
}

Config::InitialPlacePolicy Config::InitialPlacePolicy::from_str(const string_view& v) noexcept
{
    if (v == string_view(CSTR_DEF, sizeof(CSTR_DEF) / sizeof(CSTR_DEF[0]) - 1)) {
        return Config::InitialPlacePolicy(DEF);
    } else if (v == string_view(CSTR_DIRICHLET, sizeof(CSTR_DIRICHLET) / sizeof(CSTR_DIRICHLET[0]) - 1)) {
        return Config::InitialPlacePolicy(DIRICHLET);
    } else {
        FASSERT(false)
            (v)
            .what("Unknown initial-place policy.");
        return Config::InitialPlacePolicy(DEF);
    }
}

} // namespace gp
} // namespace linden
