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

#include "gpl/initialPlace.h"

#include "gpl/placerBase.h"
#include "utl/Logger.h"

namespace gpl {
using namespace std;

InitialPlaceVars::InitialPlaceVars()
{
  reset();
}

void InitialPlaceVars::reset()
{
  maxIter = 20;
  minDiffLength = 1500;
  maxSolverIter = 100;
  netWeightScale = 800.0;
  debug = false;
}

InitialPlace::InitialPlace() : ipVars_(), pb_(nullptr), log_(nullptr)
{
}

InitialPlace::InitialPlace(InitialPlaceVars ipVars,
                           std::shared_ptr<PlacerBase> pb,
                           utl::Logger* log)
    : ipVars_(ipVars), pb_(pb), log_(log)
{
}

InitialPlace::~InitialPlace()
{
  reset();
}

void InitialPlace::reset()
{
  pb_ = nullptr;
  ipVars_.reset();
}

const std::vector<Instance*>& InitialPlace::placeInsts() const
{
  return pb_->placeInsts();
}

// starting point of initial place is center.
void InitialPlace::placeInstsCenter()
{
  const int centerX = pb_->die().coreCx();
  const int centerY = pb_->die().coreCy();

  for (auto& inst : placeInsts()) {
    if (!inst->isLocked()) {
      inst->setCenterLocation(centerX, centerY);
    }
  }
}

void InitialPlace::setPlaceInstExtId()
{
  // reset ExtId for all instances
  for (auto& inst : pb_->insts()) {
    inst->setExtId(INT_MAX);
  }
  // set index only with place-able instances
  for (auto& inst : placeInsts()) {
    inst->setExtId(&inst - &(placeInsts()[0]));
  }
}

void InitialPlace::updateCoordi()
{
  for (auto& inst : placeInsts()) {
    int idx = inst->extId();
    if (!inst->isLocked()) {
      auto x = norm2absLoc(instLocVec_[X](idx), X);
      auto y = norm2absLoc(instLocVec_[Y](idx), Y);
      inst->dbSetCenterLocation(x, y);
      inst->dbSetPlaced();
    }
  }
}

// origin point of the normalized coordinates
int InitialPlace::normOriginLoc(DIMENSION dim) const
{
#if 0
  // disabled，因为normalize 到die的中心点之后，结果似乎并没有变好
  // 要和Harry进一步分析后再定是否要做这个normalize
  return dim == X ? pb_->die().dieCx() :pb_->die().dieCy();
#else
  return 0; // absolute location respect to the die's origin (0, 0)
#endif
}

float InitialPlace::norm2absLoc(float loc, DIMENSION dim) const
{
  return loc - normOriginLoc(dim);
}

float InitialPlace::abs2normLoc(float loc, DIMENSION dim) const
{
  return loc + normOriginLoc(dim);
}

std::pair<int, int> InitialPlace::minmaxLoc(DIMENSION dim) const
{
  std::pair<int, int> minmax;
  int& minLoc = minmax.first;
  int& maxLoc = minmax.second;
  auto die = pb_->die();
  minLoc = dim == X ? die.dieLx() : die.dieLy();
  maxLoc = dim == X ? die.dieUx() : die.dieUy();

  const double scale = sqrt(double(pb_->placeInstsArea()) / die.coreArea());
  const int delta = (1 - scale) * (dim == X ? die.dieDx() : die.dieDy());
  minLoc += delta;
  maxLoc -= delta;

  minLoc = abs2normLoc(minLoc, dim);
  maxLoc = abs2normLoc(maxLoc, dim);
  return minmax;
}

}  // namespace gpl
