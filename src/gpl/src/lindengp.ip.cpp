#include "gpl/lindengp.h"

#include "fassert.hpp"
#include "initialPlace.h"
#include "initialPlaceBicgstab.h"
#include "initialPlaceDef.h"
#include "initialPlaceDirichlet.h"
#include "initialPlaceEV.h"

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
  ipVars.maxFanout = 200;
  ipVars.netWeightScale = 800;
  ipVars.debug = gui_debug_initial_;

  if (initialPlacePolicy_ == "def") {
    ip_ = std::move(
        std::unique_ptr<InitialPlace>(new InitialPlaceDef(ipVars, pb_, log_)));
  } else if (initialPlacePolicy_ == "ev") {
    ip_ = std::move(
        std::unique_ptr<InitialPlace>(new InitialPlaceEV(ipVars, pb_, log_)));
  } else if (initialPlacePolicy_ == "dirichlet") {
    ip_ = std::move(std::unique_ptr<InitialPlace>(
        new InitialPlaceDirichlet(ipVars, pb_, log_)));
  } else {  // default
    ip_ = std::move(std::unique_ptr<InitialPlace>(
        new InitialPlaceBicgstab(ipVars, pb_, log_)));
  }
  ip_->run();
}

}  // namespace gpl