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

} // namespace

%} // inline
