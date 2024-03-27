#include "initial_place.hpp"
#include "dirichlet.hpp"
#include "ev/ev.hpp"
#include "ord/OpenRoad.hh"
#include "envflag/envflag.hpp"
#include "fassert.hpp"
#include <spdlog/spdlog.h>
#include <string>

IMPORT_STR_FLAG(LINDEN_INITIAL_PLACEMENT_RESULT_IN_DEF_FILE)

using namespace std;

namespace linden {

void initial_place(const linden::gp::Config& cfg, linden::db::Database& ldb) {
    spdlog::info("[linden/gpl] initialPlacePolicy: {}", cfg.initial_place_policy.to_str_view());
    switch(cfg.initial_place_policy.value) {
    case gp::Config::InitialPlacePolicy::DEF: {
        // intend to do nothing
        break;
    }
    case gp::Config::InitialPlacePolicy::DIRICHLET: {
        ip::dirichlet::run(cfg, ldb);
        break;
    }
    case gp::Config::InitialPlacePolicy::EIGEN_VEC: {
        ip::ev::run(cfg, ldb);
        break;
    }
    }
    if (auto opt = LINDEN_INITIAL_PLACEMENT_RESULT_IN_DEF_FILE(); opt) {
        const string def_out(*opt);
        spdlog::info(
            "[linden/gpl] Writing initial placement result to {}",
            def_out);
        ::ord::OpenRoad::openRoad()->writeDef(def_out.c_str(), "5.8");
    }
}

} // namespace linden
