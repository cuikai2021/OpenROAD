#include "cgr/congestionRepair.hpp"

#include "fassert.hpp"
#include "envflag/envflag.hpp"
#include "linden/lindengp.hpp"
#include "ord/OpenRoad.hh"
#include "utl/Logger.h"

using namespace std;
using utl::CGR;

IMPORT_STR_FLAG_WITH_DEFAULT(LINDEN_TMP_FILE_ROOT_DIR);

namespace cgr {

CongestionRepair::CongestionRepair() : db_(nullptr), log_(nullptr), cgr_params_file_()
{
}

void CongestionRepair::setCGRParamsFile(const char* cgr_params_file) {
  if (cgr_params_file != nullptr) {
    cgr_params_file_ = std::string(cgr_params_file);
  } else {
    cgr_params_file = "";
  }
}

void CongestionRepair::setRoutingMaxLayer(const char* max_layer) {
  if (max_layer != nullptr) {
    routing_max_layer_ = std::string(max_layer);
  } else {
    routing_max_layer_ = "";
  }
}

std::string save_def()
{
  const std::string file_name = [] {
    const std::string basename = [] {
      auto tm = std::chrono::system_clock::now().time_since_epoch();
      auto tm_ms
          = std::chrono::duration_cast<std::chrono::milliseconds>(tm).count();
      std::ostringstream oss;
      oss << LINDEN_TMP_FILE_ROOT_DIR() << "/eden_cgr_" << tm_ms;
      return oss.str();
    }();
    return basename + ".def";
  }();
  ::ord::OpenRoad::openRoad()->writeDef(file_name.c_str(), "5.8");
  return file_name;
}

double CongestionRepair::run()
{
  auto def_file = save_def();
  auto ord = ::ord::OpenRoad::openRoad();
  auto gp = ord->getLindenGlobalPlace();
  auto congestion = gp->cgr_run();
  results_.emplace_back(DEF2Congestion{def_file, congestion});
  return congestion;
}

void CongestionRepair::done() const
{
  FASSERT(results_.size() > 0);
  size_t best = 0;
  for (size_t i = 1; i < results_.size(); ++i) {
    if (results_[i].congestion < results_[best].congestion) {
      best = i;
    }
  }
  auto const& fname = results_[best].def_file;
  ::ord::OpenRoad::openRoad()->readDef(fname.c_str(), false, false,true);
}

}  // namespace cgr