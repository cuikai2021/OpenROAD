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

#include "initialPlaceBicgstab.h"

#include <Eigen/IterativeLinearSolvers>

#include "graphics.h"
#include "placerBase.h"
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

void InitialPlaceBicgstab::run()
{
  float errors[DIMENSION_CNT] = {0.0f, 0.0f};

#ifdef ENABLE_CIMG_LIB
  pe.setPlacerBase(pb_);
  pe.setLogger(log_);
  pe.Init();
#endif

  std::unique_ptr<Graphics> graphics;
  if (ipVars_.debug && Graphics::guiActive()) {
    graphics = make_unique<Graphics>(log_, pb_);
  }

  placeInstsCenter();

  // set ExtId for idx reference // easy recovery
  setPlaceInstExtId();
  for (int i = 1; i <= ipVars_.maxIter; i++) {
    updatePinInfo();
    createSparseMatrix();

    // BiCGSTAB solver for initial place
    for (auto dim : {X, Y}) {
      BiCGSTAB<SMatrix, IdentityPreconditioner> solver;
      solver.setMaxIterations(ipVars_.maxSolverIter);
      solver.compute(placeInstForceMatrix_[dim]);
      instLocVec_[dim]
          = solver.solveWithGuess(fixedInstForceVec_[dim], instLocVec_[dim]);
      errors[dim] = solver.error();
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
      graphics->cellPlot(true);
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

void InitialPlaceBicgstab::createSparseMatrix()
{
  createSparseMatrix(X);
  createSparseMatrix(Y);
}

// solve placeInstForceMatrix_[X] * xcg_x_ = xcg_b_ and placeInstForceMatrix_[Y]
// * ycg_x_ = ycg_b_ eq.
void InitialPlaceBicgstab::createSparseMatrix(DIMENSION dim)
{
  const int placeCnt = pb_->placeInsts().size();
  instLocVec_[dim].resize(placeCnt);
  fixedInstForceVec_[dim].resize(placeCnt);
  placeInstForceMatrix_[dim].resize(placeCnt, placeCnt);

  //
  // listX and listY is a temporary vector that have tuples, (idx1, idx2, val)
  //
  // listX finally becomes placeInstForceMatrix_[X]
  // listY finally becomes placeInstForceMatrix_[Y]
  //
  // The triplet vector is recommended usages
  // to fill in SparseMatrix from Eigen docs.
  //

  typedef Eigen::Triplet<float> T;
  vector<T> aList;
  aList.reserve(1000000);

  // initialize vector
  for (auto& inst : pb_->placeInsts()) {
    int idx = inst->extId();
    instLocVec_[dim](idx) = inst->c(dim);
    fixedInstForceVec_[dim](idx) = 0;
  }

  // for each net
  for (auto& net : pb_->nets()) {
    // skip for small nets.
    if (net->pins().size() <= 1) {
      continue;
    }

    // escape long time cals on huge fanout.
    //
    if (net->pins().size() >= ipVars_.maxFanout) {
      continue;
    }

    float netWeight = ipVars_.netWeightScale / (net->pins().size() - 1);

    // foreach two pins in single nets.
    auto& pins = net->pins();
    for (int pinIdx1 = 1; pinIdx1 < pins.size(); ++pinIdx1) {
      Pin* pin1 = pins[pinIdx1];
      for (int pinIdx2 = 0; pinIdx2 < pinIdx1; ++pinIdx2) {
        Pin* pin2 = pins[pinIdx2];

        // no need to fill in when instance is same
        if (pin1->instance() == pin2->instance()) {
          continue;
        }

        // B2B modeling on min/maxX pins.
        if (pin1->isMinPin(dim) || pin1->isMaxPin(dim) || pin2->isMinPin(dim)
            || pin2->isMaxPin(dim)) {
          int diff = abs(pin1->c(dim) - pin2->c(dim));
          float weight = 0;
          if (diff > ipVars_.minDiffLength) {
            weight = netWeight / diff;
          } else {
            weight = netWeight / ipVars_.minDiffLength;
          }

          // both pin cames from instance
          if (pin1->isPlaceInstConnected() && pin2->isPlaceInstConnected()) {
            const int inst1 = pin1->instance()->extId();
            const int inst2 = pin2->instance()->extId();
            // cout << "inst: " << inst1 << " " << inst2 << endl;

            aList.push_back(T(inst1, inst1, weight));
            aList.push_back(T(inst2, inst2, weight));

            aList.push_back(T(inst1, inst2, -weight));
            aList.push_back(T(inst2, inst1, -weight));

            fixedInstForceVec_[dim](inst1)
                += weight * (pin2->offsetC(dim) - pin1->offsetC(dim));

            fixedInstForceVec_[dim](inst2)
                += weight * (pin1->offsetC(dim) - pin2->offsetC(dim));
          }
          // pin1 from IO port / pin2 from Instance
          else if (!pin1->isPlaceInstConnected()
                   && pin2->isPlaceInstConnected()) {
            const int inst2 = pin2->instance()->extId();
            // cout << "inst2: " << inst2 << endl;
            aList.push_back(T(inst2, inst2, weight));
            fixedInstForceVec_[dim](inst2)
                += weight * (pin1->c(dim) - pin2->offsetC(dim));
          }
          // pin1 from Instance / pin2 from IO port
          else if (pin1->isPlaceInstConnected()
                   && !pin2->isPlaceInstConnected()) {
            const int inst1 = pin1->instance()->extId();
            // cout << "inst1: " << inst1 << endl;
            aList.push_back(T(inst1, inst1, weight));
            fixedInstForceVec_[dim](inst1)
                += weight * (pin2->c(dim) - pin1->offsetC(dim));
          }
        }
      }
    }
  }

  placeInstForceMatrix_[dim].setFromTriplets(aList.begin(), aList.end());
}

}  // namespace gpl
