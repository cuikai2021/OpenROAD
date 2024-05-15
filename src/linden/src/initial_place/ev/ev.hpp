#pragma once

#include "linden/gp.hpp"
#include "linden/db/db.hpp"

namespace linden::ip::ev {

void run(const linden::gp::Config& cfg, linden::db::Database& ldb);

} // namespace linden::ip::ev
