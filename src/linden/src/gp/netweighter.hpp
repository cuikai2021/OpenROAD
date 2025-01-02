#pragma once

namespace linden {
namespace db {
class Database;
}

int update_weights(db::Database& ldb) noexcept;

} // namespace linden