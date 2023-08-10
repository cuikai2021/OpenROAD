#pragma once

#include "load_timing_data.hpp"
#include <memory>
#include <vector>

namespace linden {

namespace db {
class Database;
}

namespace tdp {

void timing_driven_cell_move(
    std::shared_ptr<linden::db::Database> linden_db,
    const std::deque<TimingArc>& timing_arcs,
    const std::deque<Port>& ports,
    const std::deque<SeqPin>& seq_pins,
    const TdpConfig config);

}  // namespace tdp
}  // namespace linden