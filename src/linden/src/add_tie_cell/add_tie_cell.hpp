#pragma once

#include "linden/dp.hpp"
#include <deque>
#include <string>

namespace linden {

int add_tie_cell(const dp::Config& cfg, std::deque<std::string>& args, const std::string& output);

} // namespace linden
