
#include "gpl/lindengp.h"

#include <boost/algorithm/string/trim.hpp>
#include <fstream>
#include <iostream>

#include "odb/db.h"
#include "ord/OpenRoad.hh"

namespace gpl {

namespace {  // local function for -focus support
void splitNames(const std::string& line_str,
                std::vector<std::string>& names_list)
{
  // split line with delimiter ',' and skip blanks for getting names
  for (int start = 0, end = start + 1; start < line_str.size();
       start = end + 1, end = start + 1) {
    while (end < line_str.size() && line_str[end] != ',') {
      end++;
    }
    std::string name(line_str.substr(start, end - start));
    boost::algorithm::trim(name);
    names_list.emplace_back(name);
  }
  return;
}

void readConfigFile(const char* filename,
                    std::vector<std::vector<std::string>>& names)
{
  // input: configuration file;  output: focus insts/nets names -->
  // group_id(index): [name1, name2, ....]
  std::string line;
  std::ifstream config_file(filename);
  if (!config_file.is_open()) {
    std::cerr << "Could not open the configuration file: " << filename
              << std::endl;
    exit(EXIT_FAILURE);
  }
  size_t group_id = 0;
  while (getline(config_file, line)) {
    // names may contain some inst or net names in groups
    while (group_id >= names.size()) {
      names.push_back(std::vector<std::string>());
    }
    splitNames(line, names[group_id]);
    group_id++;
  }
  config_file.close();
}

}  // namespace

void LindenGlobalPlace::setFocusInsts(const char* focus_insts_file_name)
{
  readConfigFile(focus_insts_file_name, focus_inst_names_);
}

void LindenGlobalPlace::setFocusNets(const char* focus_nets_file_name)
{
  std::vector<std::vector<std::string>> focus_net_names;
  readConfigFile(focus_nets_file_name, focus_net_names);
  setFocusInstNamesFromNets(focus_net_names);
}

void LindenGlobalPlace::setFocusInstNamesFromNets(
    const std::vector<std::vector<std::string>>& net_names)
{
  auto db = ::ord::OpenRoad::openRoad()->getDb();
  auto chip = db->getChip();
  auto block = chip->getBlock();
  for (int group_id = 0; group_id < net_names.size(); group_id++) {
    while (group_id >= focus_inst_names_.size()) {
      // This loop is added to support -focus_insts and -focus_nets options at
      // the same time
      focus_inst_names_.push_back(std::vector<std::string>());
    }
    for (int cnt = 0; cnt < net_names[group_id].size(); cnt++) {
      odb::dbNet* net = block->findNet(net_names[group_id][cnt].c_str());
      if (net == nullptr) {
        fprintf(stderr,
                "net %s not found in the block!\n",
                net_names[group_id][cnt].c_str());
        continue;
      }
      for (const auto& iterm : net->getITerms()) {
        focus_inst_names_[group_id].emplace_back(iterm->getInst()->getName());
      }
    }
  }
  return;
}

}  // namespace gpl