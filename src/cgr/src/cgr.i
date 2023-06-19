%{

#include "cgr/congestionRepair.hpp"
#include "cgr/analyze_banyan_cmap_file.hpp"

namespace ord {
// Defined in OpenRoad.i
cgr::CongestionRepair* getCongestionRepair();
namespace cgr {
void analyze_banyan_cmap_file(const char* cmap_file, const char* report_file);
}

}  // namespace ord

using ord::getCongestionRepair;
using ord::cgr::analyze_banyan_cmap_file;

%}

%include "../../Exception.i"

%inline %{

namespace cgr {
  double run()
  {
    cgr::CongestionRepair* cgr = getCongestionRepair();
    return cgr->run();
  }

  void done() {
    cgr::CongestionRepair* cgr = getCongestionRepair();
    return cgr->done();
  }

  void set_cgr_params_file_cmd(const char* cgr_params_file) {
    cgr::CongestionRepair* cgr = getCongestionRepair();
    cgr->setCGRParamsFile(cgr_params_file);
  }

  void set_routing_max_layer_cmd(const char* max_layer) {
    cgr::CongestionRepair* cgr = getCongestionRepair();
    cgr->setRoutingMaxLayer(max_layer);
  }

  void analyze_banyan_cmap_file_cmd(const char* cmap_file, const char* report_file) {
    cgr::analyze_banyan_cmap_file(cmap_file, report_file);
  }

} // namespace

%} // inline
