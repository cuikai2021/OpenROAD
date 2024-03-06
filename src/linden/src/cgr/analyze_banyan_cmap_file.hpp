#pragma once

#include <string>

namespace linden {
namespace rdp {
class CongestionMap;
}  // namespace rdp
}  // namespace linden

namespace cgr {

// load banyan's cmap(congestion-map) file and save the result into cmap_result
bool load_cmap(const std::string& cmap_pb_file,
               linden::rdp::CongestionMap& cmap_result) noexcept;

// analyze banyan's cmap(congestion-map) file and report it in json
void analyze_banyan_cmap_file(const char* cmap_file,
                              const char* report_file) noexcept;

}  // namespace cgr