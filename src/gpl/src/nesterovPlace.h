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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "point.h"

namespace utl {
class Logger;
}

namespace gpl {

class PlacerBase;
class Instance;
class NesterovBase;
class RouteBase;
class TimingBase;
class Graphics;

class NesterovPlaceVars
{
 public:
  int maxNesterovIter;
  int maxBackTrack;
  float initDensityPenalty;           // INIT_LAMBDA
  float initWireLengthCoef;           // base_wcof
  float targetOverflow;               // overflow
  float minPhiCoef;                   // pcof_min
  float maxPhiCoef;                   // pcof_max
  float minPreconditioner;            // MIN_PRE
  float initialPrevCoordiUpdateCoef;  // z_ref_alpha
  float referenceHpwl;                // refDeltaHpwl
  float routabilityCheckOverflow;

  int routabilityMaxBloatIter;
  int routabilityMaxInflationIter;

  static const int maxRecursionWlCoef = 10;
  static const int maxRecursionInitSLPCoef = 10;

  bool timingDrivenMode;
  bool routabilityDrivenMode;
  bool debug;
  int debug_pause_iterations;
  int debug_update_iterations;
  bool debug_draw_bins;

  NesterovPlaceVars();
  void reset();
};

class NesterovPlace
{
 public:
  // SLP is Step Length Prediction.
  struct SLP
  {
    void resize(size_t sz);
    void swap(SLP& ano);
    void clear();
    void shrink_to_fit();
    std::vector<FloatPoint> coordi_;
    std::vector<FloatPoint> wireLengthGrads_;
    std::vector<FloatPoint> densityGrads_;
    std::vector<FloatPoint> sumGrads_;
  };

  NesterovPlace();
  NesterovPlace(NesterovPlaceVars npVars,
                std::shared_ptr<PlacerBase> pb,
                std::shared_ptr<NesterovBase> nb,
                std::shared_ptr<RouteBase> rb,
                std::shared_ptr<TimingBase> tb,
                utl::Logger* log);
  ~NesterovPlace();

  // return iteration count
  int doNesterovPlace(int start_iter = 0);

  void updateGradients(SLP& slp);

  void updateWireLengthCoef(float overflow);

  void updateInitialPrevSLPCoordi();

  float getStepLength(const SLP& prevSLP, const SLP& curSLP) const;

  void updateNextIter();
  float getPhiCoef(float scaledDiffHpwl) const;

  void updateDb();

  float getWireLengthCoefX() const { return wireLengthCoefX_; }
  float getWireLengthCoefY() const { return wireLengthCoefY_; }
  float getDensityPenalty() const { return densityPenalty_; }

  void setTargetOverflow(float overflow) { npVars_.targetOverflow = overflow; }
  void setMaxIters(int limit) { npVars_.maxNesterovIter = limit; }

 private:
#ifdef ENABLE_CIMG_LIB
  void plot(const std::string& title, int iteration);
#endif
  std::shared_ptr<PlacerBase> pb_;
  std::shared_ptr<NesterovBase> nb_;
  utl::Logger* log_;
  std::shared_ptr<RouteBase> rb_;
  std::shared_ptr<TimingBase> tb_;
  NesterovPlaceVars npVars_;
  std::unique_ptr<Graphics> graphics_;

  //
  // y_st, y_dst, y_wdst, w_pdst
  SLP curSLP_;

  // y0_st, y0_dst, y0_wdst, y0_pdst
  SLP nextSLP_;

  // z_st, z_dst, z_wdst, z_pdst
  SLP prevSLP_;

  // x_st and x0_st
  std::vector<FloatPoint> curCoordi_;
  std::vector<FloatPoint> nextCoordi_;

  // save initial coordinates -- needed for RD
  std::vector<FloatPoint> initCoordi_;

  // densityPenalty stor
  std::vector<float> densityPenaltyStor_;

  float wireLengthGradSum_;
  float densityGradSum_;

  // alpha
  float stepLength_;

  // opt_phi_cof
  float densityPenalty_;

  // base_wcof
  float baseWireLengthCoef_;

  // wlen_cof
  float wireLengthCoefX_;
  float wireLengthCoefY_;

  // phi is described in ePlace paper.
  float sumOverflow_;

  // half-parameter-wire-length
  int64_t prevHpwl_;

  float isDiverged_;
  float isRoutabilityNeed_;

  std::string divergeMsg_;
  int divergeCode_;

  int recursionCntWlCoef_;
  int recursionCntInitSLPCoef_;

  void cutFillerCoordinates();

  void init();
  void reset();
};
}  // namespace gpl
