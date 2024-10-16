#pragma once

#include <deque>
#include <string>
namespace linden {
class DpConfig;

int add_tie_cell(const DpConfig& cfg, std::deque<std::string>& args, const std::string& output);

} // namespace linden
