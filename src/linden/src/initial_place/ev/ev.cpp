#include "ev.hpp"
#include "initialPlaceEV.h"
#include <spdlog/spdlog.h>

using namespace std;

namespace linden::ip::ev {

void run(const linden::gp::Config& cfg, linden::db::Database& ldb) {
    InitialPlaceEV ev(ldb);
    ev.run();
}

} // namespace linden::ip::ev
