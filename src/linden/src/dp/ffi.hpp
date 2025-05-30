#pragma once
#include <deque>
#include <string>

namespace linden::dp {

int basic_dp_gogogo(std::deque<std::string>);
int incr_dp_gogogo(std::deque<std::string>);
int rddp_gogogo(std::deque<std::string>);
int opt_gogogo(std::deque<std::string>);
int default_config_gogogo(std::deque<std::string>);
int legalize_spare_cell_gogogo();
int check_spare_cell_gogogo();

} // namespace linden::dp
