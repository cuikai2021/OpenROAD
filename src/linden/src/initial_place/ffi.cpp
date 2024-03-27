#include "ffi.hpp"
#include "initial_place.hpp"
#include "../util/odb_util.hpp"
#include "envflag/envflag.hpp"
#include <spdlog/spdlog.h>

IMPORT_BOOL_FLAG_WITH_DEFAULT(LINDEN_SPDLOG_DISABLE_TIMESTAMP);

using namespace std;

namespace linden::ip {

void gogogo(const gp::Config& cfg) noexcept {
    if (LINDEN_SPDLOG_DISABLE_TIMESTAMP()) {
        spdlog::set_pattern("%v"); // no timestamp
    }
    spdlog::info("{}", pp::prettyPrint(cfg));

    shared_ptr<linden::db::Database> ldb = build_linden_db();
    initial_place(cfg, *ldb);
}

} // namespace linden::ip
