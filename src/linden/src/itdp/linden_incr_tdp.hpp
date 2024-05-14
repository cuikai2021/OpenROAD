#pragma once

#include "load_timing_data.hpp"
#include "linden/db/db.hpp"
#include <memory>
#include <deque>

namespace linden::itdp {

void timing_driven_cell_move(
    std::shared_ptr<linden::db::Database> linden_db,
    const std::deque<TimingArc>& timing_arcs,
    const std::deque<Port>& ports,
    const std::deque<SeqPin>& seq_pins,
    const TdpConfig config);

}  // namespace linden::itdp
