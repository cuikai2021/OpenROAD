#include "linden/lindengp.hpp"

#include <spdlog/spdlog.h>

#include "envflag/envflag.hpp"
#include "fassert.hpp"
#include "gpl/initialPlace.h"
#include "gpl/initialPlaceBicgstab.h"
#include "gpl/initialPlaceDef.h"
#include "ord/OpenRoad.hh"

IMPORT_STR_FLAG(LINDEN_INITIAL_PLACEMENT_RESULT_IN_DEF_FILE)

using namespace std;

namespace gpl {

void LindenGlobalPlace::setInitialPlacePolicy(const char* policy)
{
  initialPlacePolicy_ = policy != nullptr ? std::string(policy) : "";
}

void LindenGlobalPlace::doInitialPlace()
{
  FASSERT(pb_ != nullptr);
  std::unique_ptr<InitialPlace> ip_(nullptr);

  InitialPlaceVars ipVars;
  ipVars.maxIter = 20;
  ipVars.minDiffLength = 1500;
  ipVars.maxSolverIter = 100;
  ipVars.netWeightScale = 800;
  ipVars.debug = gui_debug_initial_;

  spdlog::info("[linden/gpl] initialPlacePolicy: {}", initialPlacePolicy_);
  if (initialPlacePolicy_ == "def") {
    ip_ = std::move(
        std::unique_ptr<InitialPlace>(new InitialPlaceDef(ipVars, pb_, log_)));
  } else {  // default
    ip_ = std::move(std::unique_ptr<InitialPlace>(
        new InitialPlaceBicgstab(ipVars, pb_, log_)));
  }
  ip_->run();
  if (auto opt = LINDEN_INITIAL_PLACEMENT_RESULT_IN_DEF_FILE(); opt) {
    const string def_out(*opt);
    spdlog::info("[linden/gpl] Writing initial placement result to {}",
                 def_out);
    ::ord::OpenRoad::openRoad()->writeDef(def_out.c_str(), "5.8");
  }
}

}  // namespace gpl