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

#include "nesterovPlace.h"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "nesterovBase.h"
#include "odb/db.h"
#include "placerBase.h"
#include "routeBase.h"
#include "timingBase.h"
#include "utl/Logger.h"
using namespace std;

#include "graphics.h"
#include "ord/Version.hh"
#include "plot.h"

namespace gpl {

using utl::GPL;

static float getDistance(const vector<FloatPoint>& a,
                         const vector<FloatPoint>& b);

static float getSecondNorm(const vector<FloatPoint>& a);

NesterovPlaceVars::NesterovPlaceVars()
{
  reset();
}

void NesterovPlaceVars::reset()
{
  maxNesterovIter = 5000;
  maxBackTrack = 10;
  initDensityPenalty = 0.00008;
  initWireLengthCoef = 0.25;
  targetOverflow = 0.1;
  minPhiCoef = 0.95;
  maxPhiCoef = 1.05;
  minPreconditioner = 1.0;
  initialPrevCoordiUpdateCoef = 100;
  referenceHpwl = 446000000;
  routabilityCheckOverflow = 0.20;
  timingDrivenMode = true;
  routabilityDrivenMode = true;
  debug = false;
  debug_pause_iterations = 10;
  debug_update_iterations = 10;
  debug_draw_bins = true;
}

NesterovPlace::NesterovPlace()
    : pb_(nullptr),
      nb_(nullptr),
      log_(nullptr),
      rb_(nullptr),
      tb_(nullptr),
      npVars_(),
      wireLengthGradSum_(0),
      densityGradSum_(0),
      stepLength_(0),
      densityPenalty_(0),
      baseWireLengthCoef_(0),
      wireLengthCoefX_(0),
      wireLengthCoefY_(0),
      sumOverflow_(0),
      prevHpwl_(0),
      isDiverged_(false),
      isRoutabilityNeed_(true),
      divergeCode_(0),
      recursionCntWlCoef_(0),
      recursionCntInitSLPCoef_(0)
{
}

NesterovPlace::NesterovPlace(NesterovPlaceVars npVars,
                             std::shared_ptr<PlacerBase> pb,
                             std::shared_ptr<NesterovBase> nb,
                             std::shared_ptr<RouteBase> rb,
                             std::shared_ptr<TimingBase> tb,
                             utl::Logger* log)
    : NesterovPlace()
{
  npVars_ = npVars;
  pb_ = pb;
  nb_ = nb;
  rb_ = rb;
  tb_ = tb;
  log_ = log;
  if (npVars.debug && Graphics::guiActive()) {
    graphics_
        = make_unique<Graphics>(log_, this, pb, nb, npVars_.debug_draw_bins);
  }
  init();
}

NesterovPlace::~NesterovPlace()
{
  reset();
}

#ifdef ENABLE_CIMG_LIB
static PlotEnv pe;
#endif

void NesterovPlace::SLP::resize(size_t sz)
{
  coordi_.resize(sz, FloatPoint());
  wireLengthGrads_.resize(sz, FloatPoint());
  densityGrads_.resize(sz, FloatPoint());
  sumGrads_.resize(sz, FloatPoint());
}

void NesterovPlace::SLP::swap(SLP& ano)
{
  // swap vector pointers
  std::swap(ano.coordi_, coordi_);
  std::swap(ano.wireLengthGrads_, wireLengthGrads_);
  std::swap(ano.densityGrads_, densityGrads_);
  std::swap(ano.sumGrads_, sumGrads_);
}

void NesterovPlace::SLP::clear()
{
  coordi_.clear();
  wireLengthGrads_.clear();
  densityGrads_.clear();
  sumGrads_.clear();
}

void NesterovPlace::SLP::shrink_to_fit()
{
  coordi_.shrink_to_fit();
  wireLengthGrads_.shrink_to_fit();
  densityGrads_.shrink_to_fit();
  sumGrads_.shrink_to_fit();
}

void NesterovPlace::init()
{
  auto gCellSize = nb_->gCells().size();
  curSLP_.resize(gCellSize);
  nextSLP_.resize(gCellSize);
  prevSLP_.resize(gCellSize);

  curCoordi_.resize(gCellSize, FloatPoint());
  nextCoordi_.resize(gCellSize, FloatPoint());
  initCoordi_.resize(gCellSize, FloatPoint());

  for (auto& gCell : nb_->gCells()) {
    nb_->updateDensityCoordiLayoutInside(gCell);
    int idx = &gCell - &nb_->gCells()[0];
    curSLP_.coordi_[idx] = prevSLP_.coordi_[idx] = curCoordi_[idx]
        = initCoordi_[idx] = FloatPoint(gCell->dCx(), gCell->dCy());
  }

  // bin
  nb_->updateGCellDensityCenterLocation(curSLP_.coordi_);

  prevHpwl_ = nb_->getHpwl();

  debugPrint(log_, GPL, "replace", 3, "npinit: InitialHPWL: {}", prevHpwl_);

  // FFT update
  nb_->updateDensityForceBin();

  baseWireLengthCoef_
      = npVars_.initWireLengthCoef
        / (static_cast<float>(nb_->binSizeX() + nb_->binSizeY()) * 0.5);

  debugPrint(log_,
             GPL,
             "replace",
             3,
             "npinit: BaseWireLengthCoef: {:g}",
             baseWireLengthCoef_);

  sumOverflow_ = nb_->densityOverflow();

  debugPrint(
      log_, GPL, "replace", 3, "npinit: OverflowArea: {}", nb_->overflowArea());
  debugPrint(log_,
             GPL,
             "replace",
             3,
             "npinit: NesterovInstArea: {}",
             nb_->nesterovInstsArea());
  debugPrint(
      log_, GPL, "replace", 3, "npinit: InitSumOverflow: {:g}", sumOverflow_);

  updateWireLengthCoef(sumOverflow_);

  debugPrint(log_,
             GPL,
             "replace",
             3,
             "npinit: WireLengthCoef: {:g}",
             wireLengthCoefX_);

  // WL update
  nb_->updateWireLengthForceWA(wireLengthCoefX_, wireLengthCoefY_);

  updateGradients(curSLP_);

  if (isDiverged_) {
    return;
  }

  // approximately fill in
  // prevSLP_.Coordi_ to calculate lc vars
  updateInitialPrevSLPCoordi();

  // bin, FFT, wlen update with prevSLPCoordi.
  nb_->updateGCellDensityCenterLocation(prevSLP_.coordi_);
  nb_->updateDensityForceBin();
  nb_->updateWireLengthForceWA(wireLengthCoefX_, wireLengthCoefY_);

  updateGradients(prevSLP_);

  if (isDiverged_) {
    return;
  }

  debugPrint(log_,
             GPL,
             "replace",
             3,
             "npinit: WireLengthGradSum {:g}",
             wireLengthGradSum_);
  debugPrint(
      log_, GPL, "replace", 3, "npinit: DensityGradSum {:g}", densityGradSum_);

  if (wireLengthGradSum_ == 0) {
    densityPenalty_ = npVars_.initDensityPenalty;
  } else {
    densityPenalty_
        = (wireLengthGradSum_ / densityGradSum_) * npVars_.initDensityPenalty;
  }

  debugPrint(log_,
             GPL,
             "replace",
             3,
             "npinit: InitDensityPenalty {:g}",
             densityPenalty_);

  sumOverflow_ = nb_->densityOverflow();

  debugPrint(
      log_, GPL, "replace", 3, "npinit: PrevSumOverflow {:g}", sumOverflow_);

  stepLength_ = getStepLength(prevSLP_, curSLP_);

  debugPrint(
      log_, GPL, "replace", 3, "npinit: InitialStepLength {:g}", stepLength_);

  if ((isnan(stepLength_) || isinf(stepLength_))
      && recursionCntInitSLPCoef_ < npVars_.maxRecursionInitSLPCoef) {
    npVars_.initialPrevCoordiUpdateCoef *= 10;
    debugPrint(log_,
               GPL,
               "replace",
               3,
               "npinit: steplength = 0 detected. Rerunning Nesterov::init() "
               "with initPrevSLPCoef {:g}",
               npVars_.initialPrevCoordiUpdateCoef);
    recursionCntInitSLPCoef_++;
    init();
  }

  if (isnan(stepLength_) || isinf(stepLength_)) {
    log_->error(GPL,
                304,
                "RePlAce diverged at initial iteration. "
                "Re-run with a smaller init_density_penalty value.");
  }
}

// clear reset
void NesterovPlace::reset()
{
  npVars_.reset();
  log_ = nullptr;

  curSLP_.clear();
  nextSLP_.clear();
  prevSLP_.clear();

  curCoordi_.clear();
  nextCoordi_.clear();

  densityPenaltyStor_.clear();

  curSLP_.shrink_to_fit();
  nextSLP_.shrink_to_fit();
  prevSLP_.shrink_to_fit();

  curCoordi_.shrink_to_fit();
  nextCoordi_.shrink_to_fit();

  densityPenaltyStor_.shrink_to_fit();

  wireLengthGradSum_ = 0;
  densityGradSum_ = 0;
  stepLength_ = 0;
  densityPenalty_ = 0;
  baseWireLengthCoef_ = 0;
  wireLengthCoefX_ = wireLengthCoefY_ = 0;
  prevHpwl_ = 0;
  isDiverged_ = false;
  isRoutabilityNeed_ = true;

  divergeMsg_ = "";
  divergeCode_ = 0;

  recursionCntWlCoef_ = 0;
  recursionCntInitSLPCoef_ = 0;
}

// to execute following function,
//
// nb_->updateGCellDensityCenterLocation(coordi); // bin update
// nb_->updateDensityForceBin(); // bin Force update
//
// nb_->updateWireLengthForceWA(wireLengthCoefX_, wireLengthCoefY_); // WL
// update
//
void NesterovPlace::updateGradients(SLP& slp)
{
  std::vector<FloatPoint>& sumGrads = slp.sumGrads_;
  std::vector<FloatPoint>& wireLengthGrads = slp.wireLengthGrads_;
  std::vector<FloatPoint>& densityGrads = slp.densityGrads_;

  debugPrint(log_,
             GPL,
             "replace",
             3,
             "updateGrad:  DensityPenalty: {:g}",
             densityPenalty_);

  // update wireLengthGrads for each gCell
  for (size_t i = 0; i < nb_->gCells().size(); i++) {
    GCell* gCell = nb_->gCells().at(i);
    wireLengthGrads[i] = nb_->getWireLengthGradientWA(
        gCell, wireLengthCoefX_, wireLengthCoefY_);
  }

  // update densityGrads for each gCell
  for (size_t i = 0; i < nb_->gCells().size(); i++) {
    GCell* gCell = nb_->gCells().at(i);
    densityGrads[i] = nb_->getDensityGradient(gCell);
  }

  // update sumGrads for each gCell
  for (size_t i = 0; i < nb_->gCells().size(); i++) {
    GCell* gCell = nb_->gCells().at(i);
    sumGrads[i].x = wireLengthGrads[i].x + densityPenalty_ * densityGrads[i].x;
    sumGrads[i].y = wireLengthGrads[i].y + densityPenalty_ * densityGrads[i].y;

    const FloatPoint& sumPrecondi = nb_->getSumPreconditioner(
        gCell, densityPenalty_, npVars_.minPreconditioner);

    sumGrads[i].x /= sumPrecondi.x;
    sumGrads[i].y /= sumPrecondi.y;
  }

  wireLengthGradSum_ = 0;
  densityGradSum_ = 0;
  float gradSum = 0;
  for (size_t i = 0; i < nb_->gCells().size(); i++) {
    // Different compiler has different results on the following formula.
    // e.g. wireLengthGradSum_ += fabs(~~.x) + fabs(~~.y);
    //
    // To prevent instability problem,
    // I partitioned the fabs(~~.x) + fabs(~~.y) as two terms.
    //
    wireLengthGradSum_ += fabs(wireLengthGrads[i].x);
    wireLengthGradSum_ += fabs(wireLengthGrads[i].y);
    densityGradSum_ += fabs(densityGrads[i].x);
    densityGradSum_ += fabs(densityGrads[i].y);
    gradSum += fabs(sumGrads[i].x);
    gradSum += fabs(sumGrads[i].y);
  }

  debugPrint(log_,
             GPL,
             "replace",
             3,
             "updateGrad:  WireLengthGradSum: {:g}",
             wireLengthGradSum_);
  debugPrint(log_,
             GPL,
             "replace",
             3,
             "updateGrad:  DensityGradSum: {:g}",
             densityGradSum_);
  debugPrint(log_, GPL, "replace", 3, "updateGrad:  GradSum: {:g}", gradSum);

  // sometimes wirelength gradient is zero when design is too small
  if (wireLengthGradSum_ == 0
      && recursionCntWlCoef_ < npVars_.maxRecursionWlCoef) {
    wireLengthCoefX_ *= 0.5;
    wireLengthCoefY_ *= 0.5;
    baseWireLengthCoef_ *= 0.5;
    debugPrint(log_,
               GPL,
               "replace",
               3,
               "updateGrad:  sum(WL gradient) = 0 detected, trying again with "
               "wlCoef: {:g} {:g}",
               wireLengthCoefX_,
               wireLengthCoefY_);

    // update WL forces
    nb_->updateWireLengthForceWA(wireLengthCoefX_, wireLengthCoefY_);

    // recursive call again with smaller wirelength coef
    recursionCntWlCoef_++;
    updateGradients(slp);
  }

  // divergence detection on
  // Wirelength / density gradient calculation
  if (isinf(densityGradSum_)) {
    isDiverged_ = true;
    divergeMsg_ = "RePlAce diverged at wire/density gradient Sum.";
    divergeCode_ = 306;
  }
}

int NesterovPlace::doNesterovPlace(int start_iter)
{
  // if replace diverged in init() function,
  // replace must be skipped.
  if (isDiverged_) {
    log_->error(GPL, divergeCode_, divergeMsg_);
    return 0;
  }

#ifdef ENABLE_CIMG_LIB
  pe.setPlacerBase(pb_);
  pe.setNesterovBase(nb_);
  pe.setLogger(log_);
  pe.Init();
  plot("Nesterov - BeforeStart", 0);
#endif

  if (graphics_) {
    graphics_->cellPlot(true);
  }

  // backTracking variable.
  float curA = 1.0;

  // divergence detection
  float minSumOverflow = 1e30;
  float hpwlWithMinSumOverflow = 1e30;

  // dynamic adjustment of max_phi_coef
  bool isMaxPhiCoefChanged = false;

  // snapshot saving detection
  bool isSnapshotSaved = false;

  // snapshot info
  vector<FloatPoint> snapshotCoordi;
  vector<FloatPoint> snapshotSLPCoordi;
  vector<FloatPoint> snapshotSLPSumGrads;
  float snapshotA = 0;
  float snapshotDensityPenalty = 0;
  float snapshotStepLength = 0;
  float snapshotWlCoefX = 0, snapshotWlCoefY = 0;

  bool isDivergeTriedRevert = false;

  // Core Nesterov Loop
  int iter = start_iter;
  for (; iter < npVars_.maxNesterovIter; iter++) {
    debugPrint(log_, GPL, "replace", 3, "np:  Iter: {}", iter + 1);

    float prevA = curA;

    // here, prevA is a_(k), curA is a_(k+1)
    // See, the ePlace paper's Algorithm 2: Nesterov-Solver at kth iteration
    //
    curA = (1.0 + sqrt(4.0 * prevA * prevA + 1.0)) * 0.5;

    // coeff is (a_k - 1) / ( a_(k+1) ) in paper.
    float coeff = (prevA - 1.0) / curA;

    debugPrint(log_, GPL, "replace", 3, "np:  PreviousA: {:g}", prevA);
    debugPrint(log_, GPL, "replace", 3, "np:  CurrentA: {:g}", curA);
    debugPrint(log_, GPL, "replace", 3, "np:  Coefficient: {:g}", coeff);
    debugPrint(log_, GPL, "replace", 3, "np:  StepLength: {:g}", stepLength_);

    // Back-Tracking loop
    int numBackTrak = 0;
    for (numBackTrak = 0; numBackTrak < npVars_.maxBackTrack; numBackTrak++) {
      // fill in nextCoordinates with given stepLength_
      for (size_t k = 0; k < nb_->gCells().size(); k++) {
        // new solution u_(k+1)
        FloatPoint nextCoordi(
            curSLP_.coordi_[k].x + stepLength_ * curSLP_.sumGrads_[k].x,
            curSLP_.coordi_[k].y + stepLength_ * curSLP_.sumGrads_[k].y);

        // new reference solution v_(k+1)
        FloatPoint nextSLPCoordi(
            nextCoordi.x + coeff * (nextCoordi.x - curCoordi_[k].x),
            nextCoordi.y + coeff * (nextCoordi.y - curCoordi_[k].y));

        GCell* curGCell = nb_->gCells()[k];

        nextCoordi_[k] = FloatPoint(
            nb_->getDensityCoordiLayoutInsideX(curGCell, nextCoordi.x),
            nb_->getDensityCoordiLayoutInsideY(curGCell, nextCoordi.y));

        nextSLP_.coordi_[k] = FloatPoint(
            nb_->getDensityCoordiLayoutInsideX(curGCell, nextSLPCoordi.x),
            nb_->getDensityCoordiLayoutInsideY(curGCell, nextSLPCoordi.y));
      }

      nb_->updateGCellDensityCenterLocation(nextSLP_.coordi_);
      nb_->updateDensityForceBin();
      nb_->updateWireLengthForceWA(wireLengthCoefX_, wireLengthCoefY_);

      updateGradients(nextSLP_);

      // NaN or inf is detected in WireLength/Density Coef
      if (isDiverged_) {
        break;
      }

      float newStepLength = getStepLength(curSLP_, nextSLP_);

      debugPrint(
          log_, GPL, "replace", 3, "np:  NewStepLength: {:g}", newStepLength);

      if (isnan(newStepLength) || isinf(newStepLength)) {
        isDiverged_ = true;
        divergeMsg_ = "RePlAce diverged at newStepLength.";
        divergeCode_ = 305;
        break;
      }

      if (newStepLength > stepLength_ * 0.95) {
        stepLength_ = newStepLength;
        break;
      } else if (newStepLength < 0.01) {
        stepLength_ = 0.01;
        break;
      } else {
        stepLength_ = newStepLength;
      }
    }

    debugPrint(
        log_, GPL, "replace", 3, "np:  NumBackTrak: {}", numBackTrak + 1);

    // dynamic adjustment for
    // better convergence with
    // large designs
    if (!isMaxPhiCoefChanged && sumOverflow_ < 0.35f) {
      isMaxPhiCoefChanged = true;
      npVars_.maxPhiCoef *= 0.99;
    }

    if (npVars_.maxBackTrack == numBackTrak) {
      debugPrint(log_,
                 GPL,
                 "replace",
                 3,
                 "Backtracking limit reached so a small step will be taken");
    }

    if (isDiverged_) {
      break;
    }

    updateNextIter();

    // For JPEG Saving
    // debug

    if (graphics_) {
      bool update
          = (iter == 0 || (iter + 1) % npVars_.debug_update_iterations == 0);
      if (update) {
        bool pause
            = (iter == 0 || (iter + 1) % npVars_.debug_pause_iterations == 0);
        graphics_->cellPlot(pause);
      }
    }

    if (iter == 0 || (iter + 1) % 10 == 0) {
      log_->report("[NesterovSolve] Iter: {} overflow: {:g} HPWL: {}",
                   iter + 1,
                   sumOverflow_,
                   prevHpwl_);

#ifdef ENABLE_CIMG_LIB
      plot("Nesterov - Iter: " + std::to_string(iter + 1), iter + 1);
#endif
    }

    if (minSumOverflow > sumOverflow_) {
      minSumOverflow = sumOverflow_;
      hpwlWithMinSumOverflow = prevHpwl_;
    }

    // timing driven feature
    // do reweight on timing-critical nets.
    if (npVars_.timingDrivenMode
        && tb_->isTimingNetWeightOverflow(sumOverflow_)) {
      // update db's instance location from current density coordinates
      updateDb();

      // Call resizer's estimateRC API to fill in PEX using placed locations,
      // Call sta's API to extract worst timing paths,
      // and update GNet's weights from worst timing paths.
      //
      // See timingBase.cpp in detail
      bool shouldTdProceed = tb_->updateGNetWeights(sumOverflow_);

      // problem occured
      // escape timing driven later
      if (!shouldTdProceed) {
        npVars_.timingDrivenMode = false;
      }
    }

    // diverge detection on
    // large max_phi_cof value + large design
    //
    // 1) happen overflow < 20%
    // 2) Hpwl is growing
    //
    if (sumOverflow_ < 0.3f && sumOverflow_ - minSumOverflow >= 0.02f
        && hpwlWithMinSumOverflow * 1.2f < prevHpwl_) {
      divergeMsg_ = "RePlAce divergence detected. ";
      divergeMsg_ += "Re-run with a smaller max_phi_cof value.";
      divergeCode_ = 307;
      isDiverged_ = true;

      // revert back to the original rb solutions
      // one more opportunity
      if (!isDivergeTriedRevert && rb_->numCall() >= 1) {
        // get back to the working rc size
        rb_->revertGCellSizeToMinRc();

        // revert back the current density penality
        curCoordi_ = snapshotCoordi;
        curSLP_.coordi_ = snapshotSLPCoordi;
        curSLP_.sumGrads_ = snapshotSLPSumGrads;
        curA = snapshotA;
        densityPenalty_ = snapshotDensityPenalty;
        stepLength_ = snapshotStepLength;
        wireLengthCoefX_ = snapshotWlCoefX;
        wireLengthCoefY_ = snapshotWlCoefY;

        nb_->updateGCellDensityCenterLocation(snapshotCoordi);
        nb_->updateDensityForceBin();
        nb_->updateWireLengthForceWA(wireLengthCoefX_, wireLengthCoefY_);

        isDiverged_ = false;
        divergeCode_ = 0;
        divergeMsg_ = "";
        isDivergeTriedRevert = true;

        // turn off the RD forcely
        isRoutabilityNeed_ = false;
      } else {
        // no way to revert
        break;
      }
    }

    // save snapshots for routability-driven
    if (!isSnapshotSaved && npVars_.routabilityDrivenMode
        && 0.6 >= sumOverflow_) {
      snapshotCoordi = curCoordi_;
      snapshotSLPCoordi = curSLP_.coordi_;
      snapshotSLPSumGrads = curSLP_.sumGrads_;
      snapshotA = curA;
      snapshotDensityPenalty = densityPenalty_;
      snapshotStepLength = stepLength_;
      snapshotWlCoefX = wireLengthCoefX_;
      snapshotWlCoefY = wireLengthCoefY_;

      isSnapshotSaved = true;
      log_->report("[NesterovSolve] Snapshot saved at iter = {}", iter);
    }

    // check routability using GR
    if (npVars_.routabilityDrivenMode && isRoutabilityNeed_
        && npVars_.routabilityCheckOverflow >= sumOverflow_) {
      // recover the densityPenalty values
      // if further routability-driven is needed
      std::pair<bool, bool> result = rb_->routability();
      isRoutabilityNeed_ = result.first;
      bool isRevertInitNeeded = result.second;

      // if routability is needed
      if (isRoutabilityNeed_ || isRevertInitNeeded) {
        // cutFillerCoordinates();

        // revert back the current density penality
        curCoordi_ = snapshotCoordi;
        curSLP_.coordi_ = snapshotSLPCoordi;
        curSLP_.sumGrads_ = snapshotSLPSumGrads;
        curA = snapshotA;
        densityPenalty_ = snapshotDensityPenalty;
        stepLength_ = snapshotStepLength;
        wireLengthCoefX_ = snapshotWlCoefX;
        wireLengthCoefY_ = snapshotWlCoefY;

        nb_->updateGCellDensityCenterLocation(snapshotCoordi);
        nb_->updateDensityForceBin();
        nb_->updateWireLengthForceWA(wireLengthCoefX_, wireLengthCoefY_);

        // reset the divergence detect conditions
        minSumOverflow = 1e30;
        hpwlWithMinSumOverflow = 1e30;
        log_->report("[NesterovSolve] Revert back to snapshot coordi");
      }
    }

    // if it reached target overflow
    if (sumOverflow_ <= npVars_.targetOverflow) {
      log_->report("[NesterovSolve] Finished with Overflow: {:.6f}",
                   sumOverflow_);
      debugPrint(log_,
                 GPL,
                 "replace",
                 1,
                 "[NesterovSolve] Finished with Iter={} overflow:={:g} "
                 "HPWL={}, eden_version={}",
                 iter + 1,
                 sumOverflow_,
                 nb_->getHpwl(),
                 EDEN_VERSION);
      break;
    }
  }

  log_->metric("gp::iter", iter+1);

  // in all case including diverge,
  // db should be updated.
  updateDb();

  if (isDiverged_) {
    log_->error(GPL, divergeCode_, divergeMsg_);
  }

  if (graphics_) {
    graphics_->status("End placement");
    graphics_->cellPlot(true);
  }
  return iter;
}

// wlCoef = 1/γ， where γ is calculated with formula (38)
// in the ePlace paper
//    γ = base * pow(10, kτ+b), where τ is the density overflow value.
//    , set γ(τ=1.0) = base * 10, and γ(τ=1.0) = base * 0.1
//    , we can get k = 20/9, b = -11/9
void NesterovPlace::updateWireLengthCoef(float overflow)
{
  if (overflow > 1.0) {
    wireLengthCoefX_ = wireLengthCoefY_ = 0.1;
  } else if (overflow < 0.1) {
    wireLengthCoefX_ = wireLengthCoefY_ = 10.0;
  } else {
    wireLengthCoefX_ = wireLengthCoefY_
        = 1.0 / pow(10.0, (overflow - 0.1) * 20 / 9.0 - 1.0);
  }

  wireLengthCoefX_ *= baseWireLengthCoef_;
  wireLengthCoefY_ *= baseWireLengthCoef_;
  debugPrint(log_,
             GPL,
             "replace",
             3,
             "updateWireLengthCoef:  NewWireLengthCoef: {:g}",
             wireLengthCoefX_);
}

void NesterovPlace::updateInitialPrevSLPCoordi()
{
  for (size_t i = 0; i < nb_->gCells().size(); i++) {
    GCell* curGCell = nb_->gCells()[i];

    float prevCoordiX
        = curSLP_.coordi_[i].x
          - npVars_.initialPrevCoordiUpdateCoef * curSLP_.sumGrads_[i].x;

    float prevCoordiY
        = curSLP_.coordi_[i].y
          - npVars_.initialPrevCoordiUpdateCoef * curSLP_.sumGrads_[i].y;

    FloatPoint newCoordi(
        nb_->getDensityCoordiLayoutInsideX(curGCell, prevCoordiX),
        nb_->getDensityCoordiLayoutInsideY(curGCell, prevCoordiY));

    prevSLP_.coordi_[i] = newCoordi;
  }
}

void NesterovPlace::updateNextIter()
{
  prevSLP_.swap(curSLP_);

  // Prevent locked instances from moving
  const auto& gCells = nb_->gCells();
  for (size_t k = 0; k < gCells.size(); ++k) {
    if (gCells[k]->isInstance() && gCells[k]->instance()->isLocked()) {
      nextSLP_.coordi_[k] = curSLP_.coordi_[k];
      nextSLP_.wireLengthGrads_[k] = curSLP_.wireLengthGrads_[k];
      nextSLP_.densityGrads_[k] = curSLP_.densityGrads_[k];
      nextSLP_.sumGrads_[k] = curSLP_.sumGrads_[k];

      nextCoordi_[k] = curCoordi_[k];
    }
  }

  curSLP_.swap(nextSLP_);

  std::swap(curCoordi_, nextCoordi_);

  sumOverflow_ = nb_->densityOverflow();

  debugPrint(log_,
             GPL,
             "replace",
             3,
             "updateNextIter:  Gradient: {:g}",
             getSecondNorm(curSLP_.sumGrads_));
  debugPrint(
      log_, GPL, "replace", 3, "updateNextIter:  Phi: {:g}", nb_->sumPhi());
  debugPrint(
      log_, GPL, "replace", 3, "updateNextIter:  Overflow: {:g}", sumOverflow_);

  updateWireLengthCoef(sumOverflow_);
  int64_t hpwl = nb_->getHpwl();

  debugPrint(
      log_, GPL, "replace", 3, "updateNextIter:  PreviousHPWL: {}", prevHpwl_);
  debugPrint(log_, GPL, "replace", 3, "updateNextIter:  NewHPWL: {}", hpwl);

  float phiCoef = getPhiCoef(static_cast<float>(hpwl - prevHpwl_)
                             / npVars_.referenceHpwl);

  prevHpwl_ = hpwl;
  densityPenalty_ *= phiCoef;

  debugPrint(
      log_, GPL, "replace", 3, "updateNextIter:  PhiCoef: {:g}", phiCoef);

  // for routability densityPenalty recovery
  if (rb_->numCall() == 0) {
    densityPenaltyStor_.push_back(densityPenalty_);
  }
}

// step-length = 1 / Lipschitz-constant
// Details is in Algorithm-2 Nesterov-Solver at kth iteration in the ePlace
// Paper And the step-length α_k = 1/ L_k, where L_k is the equaration (29) in
// the Paper
float NesterovPlace::getStepLength(const SLP& prevSLP, const SLP& curSLP) const
{
  float coordiDistance = getDistance(prevSLP.coordi_, curSLP.coordi_);
  float gradDistance = getDistance(prevSLP.sumGrads_, curSLP.sumGrads_);

  debugPrint(log_,
             GPL,
             "replace",
             3,
             "getStepLength:  CoordinateDistance: {:g}",
             coordiDistance);
  debugPrint(log_,
             GPL,
             "replace",
             3,
             "getStepLength:  GradientDistance: {:g}",
             gradDistance);

  return coordiDistance / gradDistance;
}

// λ Scaling coef
// Algorithm-2 in RePlAce paper
float NesterovPlace::getPhiCoef(float scaledDiffHpwl) const
{
  debugPrint(log_,
             GPL,
             "replace",
             3,
             "getPhiCoef:  InputScaleDiffHPWL: {:g}",
             scaledDiffHpwl);

  float retCoef = (scaledDiffHpwl < 0)
                      ? npVars_.maxPhiCoef
                      : npVars_.maxPhiCoef
                            * pow(npVars_.maxPhiCoef, scaledDiffHpwl * -1.0);
  retCoef = std::max(npVars_.minPhiCoef, retCoef);
  return retCoef;
}

void NesterovPlace::updateDb()
{
  nb_->updateDbGCells();
}

void NesterovPlace::cutFillerCoordinates()
{
  curSLP_.resize(nb_->fillerCnt());
  nextSLP_.resize(nb_->fillerCnt());
  prevSLP_.resize(nb_->fillerCnt());

  curCoordi_.resize(nb_->fillerCnt());
  nextCoordi_.resize(nb_->fillerCnt());
}

static float getDistance(const vector<FloatPoint>& a,
                         const vector<FloatPoint>& b)
{
  float sumDistance = 0.0f;
  for (size_t i = 0; i < a.size(); i++) {
    sumDistance += (a[i].x - b[i].x) * (a[i].x - b[i].x);
    sumDistance += (a[i].y - b[i].y) * (a[i].y - b[i].y);
  }
  return sqrt(sumDistance / (2.0 * a.size()));
}

static float getSecondNorm(const vector<FloatPoint>& a)
{
  float norm = 0;
  for (auto& coordi : a) {
    norm += coordi.x * coordi.x + coordi.y * coordi.y;
  }
  return sqrt(norm / (2.0 * a.size()));
}

#ifdef ENABLE_CIMG_LIB
void NesterovPlace::plot(const std::string& title, int iteration)
{
  if (PlotEnv::isPlotEnabled()) {
    std::ostringstream iter_str;
    iter_str << std::setw(4) << std::setfill('0') << iteration;

    pe.SaveCellPlotAsJPEG(title, true, "cell_" + iter_str.str());
    pe.SaveBinPlotAsJPEG(title, "bin_" + iter_str.str());
    pe.SaveArrowPlotAsJPEG(title, "arrow_" + iter_str.str());
  }
}
#endif

}  // namespace gpl
