#pragma once

#include <memory>

namespace odb {
class dbDatabase;
} // namespace odb

namespace linden {
namespace db {
class Database;
}
}

namespace itdp {

// FIXME: add constraints to block moving cells into banned-areas, like blockages|fixed-macros|halos.
class LPBasedPostDrvItdp {
public:
    LPBasedPostDrvItdp();
    void run(const char* pb_path);

private:
    std::shared_ptr<linden::db::Database> create_linden_db() noexcept;
    // FIXME: remove this member variable.
    odb::dbDatabase* db_;

};

} // namespace itdp
