///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (c) 2018-2020, The Regents of the University of California
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///////////////////////////////////////////////////////////////////////////////

#include "gpl/initialPlaceBicgstab.h"

#include <Eigen/IterativeLinearSolvers>

#include "gpl/graphics.h"
#include "netModelB2B.h"
#include "gpl/placerBase.h"
#include "plot.h"
#include "utl/Logger.h"

namespace gpl {
using namespace std;

using Eigen::BiCGSTAB;
using Eigen::IdentityPreconditioner;
using utl::GPL;

#ifdef ENABLE_CIMG_LIB
static PlotEnv pe;
#endif

InitialPlaceBicgstab::InitialPlaceBicgstab(InitialPlaceVars ipVars,
                                           std::shared_ptr<PlacerBase> pb,
                                           utl::Logger* logger)
    : InitialPlace(ipVars, pb, logger)
{
  placeInstsCenter();

  // set ExtId for idx reference // easy recovery
  setPlaceInstExtId();
}

// Solve two SparseMatrix on X/Y equations here;
//
// find instance location vector X
// s.t. satisfies AX = b, where
//   - A: SparseMatrix that contains connectivity forces on X // B2B model is
//   used
//   - X: current/target instances' center X coordinates. 1-col vector.
//   - b: contains fixed instances' forces toward X coordi. 1-col vector.
//
// Used the interative BiCGSTAB solver to solve matrix eqs.
void InitialPlaceBicgstab::run()
{
#ifdef ENABLE_CIMG_LIB
  pe.setPlacerBase(pb_);
  pe.setLogger(log_);
  pe.Init();
#endif

  std::unique_ptr<Graphics> graphics;
  if (ipVars_.debug && Graphics::guiActive()) {
    graphics = make_unique<Graphics>(log_, pb_);
  }

  float errors[DIMENSION_CNT] = {0.0f, 0.0f};
  IsScanChainsIgnoredPin is_scan_chains_ignored_pin(pb_->db()->getChip()->getBlock()->getScanChains());
  IgnoredNets ignored_nets;
  for (int i = 1; i <= ipVars_.maxIter; i++) {
    updatePinInfo();
    NetModelB2B model(ipVars_.minDiffLength,
                      ipVars_.netWeightScale,
                      is_scan_chains_ignored_pin,
                      ignored_nets,
                      pb_->nets(),
                      placeInsts());
    for (auto dim : {X, Y}) {
      SMatrix s(placeInstsNum(), placeInstsNum());
      // The triplet vector is recommended usages to fill in SparseMatrix from
      // Eigen docs.
      auto matrixATriplets = model.getMatrixATriplets(dim);
      s.setFromTriplets(matrixATriplets.begin(), matrixATriplets.end());

      // initialize vector
      instLocVec_[dim].resize(placeInstsNum());
      for (auto& inst : placeInsts()) {
        auto idx = inst->extId();
        instLocVec_[dim](idx) = inst->c(dim);
      }
      errors[dim] = bicgSolve(s, model.getVectorB(dim), instLocVec_[dim]);
    }

    log_->report("[InitialPlace]  Iter: {} CG Error: {:0.8f} HPWL: {}",
                 i,
                 max(errors[X], errors[Y]),
                 pb_->hpwl());
    updateCoordi();

#ifdef ENABLE_CIMG_LIB
    if (PlotEnv::isPlotEnabled())
      pe.SaveCellPlotAsJPEG(string("InitPlace ") + to_string(i),
                            false,
                            string("ip_") + to_string(i));
#endif

    if (graphics) {
      graphics->cellPlot(false);
    }

    if (max(errors[X], errors[Y]) <= 1e-5 && i >= 5) {
      break;
    }
  }
}

void InitialPlaceBicgstab::updatePinInfo()
{
  updatePinInfo(X);
  updatePinInfo(Y);
}

void InitialPlaceBicgstab::updatePinInfo(DIMENSION dim)
{
  // reset all MinMax attributes
  for (auto& pin : pb_->pins()) {
    pin->unsetMinPin(dim);
    pin->unsetMaxPin(dim);
  }

  for (auto& net : pb_->nets()) {
    Pin* pinMin = nullptr;
    Pin* pinMax = nullptr;
    int min = INT_MAX;
    int max = INT_MIN;

    // Mark B2B info on Pin structures
    for (auto& pin : net->pins()) {
      if (min > pin->c(dim)) {
        if (pinMin) {
          pinMin->unsetMinPin(dim);
        }
        min = pin->c(dim);
        pinMin = pin;
        pinMin->setMinPin(dim);
      }

      if (max < pin->c(dim)) {
        if (pinMax) {
          pinMax->unsetMaxPin(dim);
        }
        max = pin->c(dim);
        pinMax = pin;
        pinMax->setMaxPin(dim);
      }
    }
  }
}

float InitialPlaceBicgstab::bicgSolve(const SMatrix& L,
                                      const Eigen::VectorXf& b,
                                      Eigen::VectorXf& instLocs)
{
  BiCGSTAB<SMatrix, IdentityPreconditioner> solver;
  solver.setMaxIterations(ipVars_.maxSolverIter);
  solver.compute(L);
  instLocs = solver.solveWithGuess(b, instLocs);
  return solver.error();
}

}  // namespace gpl
