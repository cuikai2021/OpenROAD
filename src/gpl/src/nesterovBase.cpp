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

#include "nesterovBase.h"

#include <algorithm>
#include <iostream>
#include <random>

#include "fft.h"
#include "odb/db.h"
#include "gpl/placerBase.h"
#include "util.h"
#include "utl/Logger.h"

#define REPLACE_SQRT2 1.414213562373095048801L

namespace gpl {

using utl::GPL;

using namespace std;
using namespace odb;

static int64_t getOverlapArea(const Bin* bin, const Instance* inst);

// Note that
// int64_t is ideal in the following function, but
// runtime is doubled compared with float.
//
// Choose to use "float" only in the following functions
static float getOverlapDensityArea(const Bin* bin, const GCell* cell);

static float fastExp(float exp);

////////////////////////////////////////////////
// GCell

GCell::GCell()
    : IntRect(),
      inst_(nullptr),
      dRect_(),
      densityScale_(1.0),
      gradientX_(0),
      gradientY_(0)
{
}

GCell::GCell(Instance* inst) : GCell()
{
  setInstance(inst);
}

GCell::GCell(const odb::Point& c, int dx, int dy) : GCell()
{
  int lx = c.getX() - dx / 2;
  int ly = c.getY() - dy / 2;
  int ux = c.getX() + dx / 2;
  int uy = c.getY() + dy / 2;
  dRect_.rect() = rect() = Rect(lx, ly, ux, uy);

  setFiller();
}

GCell::~GCell()
{
}

void GCell::setInstance(Instance* inst)
{
  assert(inst);
  inst_ = inst;
  // density coordi has the same center points.
  dRect_.rect() = rect() = inst->rect();
}

Instance* GCell::instance() const
{
  return inst_;
}

void GCell::addGPin(GPin* gPin)
{
  gPins_.push_back(gPin);
}

// do nothing
void GCell::setFiller()
{
  assert(inst_ == nullptr);
}

void GCell::setLocation(int lx, int ly)
{
  rect().moveTo(lx, ly);

  for (auto& gPin : gPins_) {
    gPin->updateLocation(this);
  }
}

void GCell::setCenterLocation(int x, int y)
{
  rect().moveDelta(x - cx(), y - cy());

  for (auto& gPin : gPins_) {
    gPin->updateLocation(this);
  }
}

void GCell::setSize(int dx, int dy)
{
  resize(dx, dy);
}

void GCell::setDensityLocation(int dLx, int dLy)
{
  dRect_.rect().moveTo(dLx, dLy);

  // assume that density Center change the gPin coordi
  for (auto& gPin : gPins_) {
    gPin->updateDensityLocation(this);
  }
}

void GCell::setDensityCenterLocation(int x, int y)
{
  dRect_.rect().moveDelta(x - dCx(), y - dCy());

  // assume that density Center change the gPin coordi
  for (auto& gPin : gPins_) {
    gPin->updateDensityLocation(this);
  }
}

void GCell::setDensitySize(int dDx, int dDy)
{
  dRect_.resize(dDx, dDy);
#if 0
  if (this->isInstance()) {
    std::cout << this->instance()->dbInst()->getName() << " eden:(dx, dy)=("
              << dRect_.dx() << ", " << dRect_.dy() << "),  "
              << ":(cx, cy)=(" << dRect_.cx() << ", " << dRect_.cy() << ")"
              << ", (dDx, dDy)=(" << dDx << ", " << dDy << ")"
              << ", (lx, ly)=(" << dRect_.lx() << ", " << dRect_.ly() << ")"
              << ", (ux, uy)=(" << dRect_.ux() << ", " << dRect_.uy() << ")"
              << std::endl;
  }
#endif
}

void GCell::setDensityScale(float densityScale)
{
  densityScale_ = densityScale;
#if 0
  if (this->isInstance()) {
    std::cout << this->instance()->dbInst()->getName() << ":densityScale_=" << densityScale_
              << std::endl;
  }
#endif
}

void GCell::setGradientX(float gradientX)
{
  gradientX_ = gradientX;
}

void GCell::setGradientY(float gradientY)
{
  gradientY_ = gradientY;
}

bool GCell::isInstance() const
{
  return inst_ != nullptr;
}

bool GCell::isFiller() const
{
  return inst_ == nullptr;
}

bool GCell::isMacroInstance() const
{
  if (!isInstance()) {
    return false;
  }
  return instance()->isMacro();
}

bool GCell::isStdInstance() const
{
  if (!isInstance()) {
    return false;
  }
  return !instance()->isMacro();
}

////////////////////////////////////////////////
// GNet

GNet::GNet()
    : IntRect(),
      net_(nullptr),
      timingWeight_(1),
      customWeight_(1),
      isDontCare_(false)
{
}

GNet::GNet(Net* net) : GNet()
{
  net_ = net;
}

GNet::~GNet()
{
  gPins_.clear();
  net_ = nullptr;
}

void GNet::setTimingWeight(float timingWeight)
{
  timingWeight_ = timingWeight;
}

void GNet::setCustomWeight(float customWeight)
{
  customWeight_ = customWeight;
}

void GNet::addGPin(GPin* gPin)
{
  gPins_.push_back(gPin);
}

void GNet::updateBox()
{
  rect_.reset(INT_MAX, INT_MAX, INT_MIN, INT_MIN);
  for (auto& gPin : gPins_) {
    rect_.reset(std::min(gPin->cx(), lx()),
                std::min(gPin->cy(), ly()),
                std::max(gPin->cx(), ux()),
                std::max(gPin->cy(), uy()));
  }
}

int64_t GNet::hpwl() const
{
  return dx() + dy();
}

void GNet::clearWaVars()
{
  wa_[X].clear();
  wa_[Y].clear();
}

void GNet::setDontCare()
{
  isDontCare_ = true;
}

bool GNet::isDontCare() const
{
  return (gPins_.size() == 0) || isDontCare_;
}

////////////////////////////////////////////////
// GPin

GPin::GPin()
    : pin_(nullptr),
      gCell_(nullptr),
      gNet_(nullptr),
      offsetCx_(0),
      offsetCy_(0),
      cx_(0),
      cy_(0),
      maxExpSumX_(0),
      maxExpSumY_(0),
      minExpSumX_(0),
      minExpSumY_(0),
      hasMaxExpSumX_(0),
      hasMaxExpSumY_(0),
      hasMinExpSumX_(0),
      hasMinExpSumY_(0)
{
}

GPin::GPin(Pin* pin) : GPin()
{
  assert(pin);
  pin_ = pin;
  cx_ = pin->cx();
  cy_ = pin->cy();
  offsetCx_ = pin->offsetCx();
  offsetCy_ = pin->offsetCy();
}

GPin::~GPin()
{
  gCell_ = nullptr;
  gNet_ = nullptr;
  pin_ = nullptr;
}

Pin* GPin::pin() const
{
  return pin_;
}

void GPin::setGCell(GCell* gCell)
{
  gCell_ = gCell;
}

void GPin::setGNet(GNet* gNet)
{
  gNet_ = gNet;
}

void GPin::setCenterLocation(int cx, int cy)
{
  cx_ = cx;
  cy_ = cy;
}

void GPin::clearWaVars()
{
  hasMaxExpSumX_ = 0;
  hasMaxExpSumY_ = 0;
  hasMinExpSumX_ = 0;
  hasMinExpSumY_ = 0;

  maxExpSumX_ = maxExpSumY_ = 0;
  minExpSumX_ = minExpSumY_ = 0;
}

void GPin::setMaxExpSumX(float maxExpSumX)
{
  hasMaxExpSumX_ = 1;
  maxExpSumX_ = maxExpSumX;
}

void GPin::setMaxExpSumY(float maxExpSumY)
{
  hasMaxExpSumY_ = 1;
  maxExpSumY_ = maxExpSumY;
}

void GPin::setMinExpSumX(float minExpSumX)
{
  hasMinExpSumX_ = 1;
  minExpSumX_ = minExpSumX;
}

void GPin::setMinExpSumY(float minExpSumY)
{
  hasMinExpSumY_ = 1;
  minExpSumY_ = minExpSumY;
}

void GPin::updateLocation(const GCell* gCell)
{
  cx_ = gCell->cx() + offsetCx_;
  cy_ = gCell->cy() + offsetCy_;
}

void GPin::updateDensityLocation(const GCell* gCell)
{
  cx_ = gCell->dCx() + offsetCx_;
  cy_ = gCell->dCy() + offsetCy_;
}

////////////////////////////////////////////////////////
// Bin

Bin::Bin()
    : index_(0, 0),
      nonPlaceArea_(0),
      instPlacedArea_(0),
      fillerArea_(0),
      density_(0),
      targetDensity_(0),
      electroPhi_(0),
      electroForceX_(0),
      electroForceY_(0)
{
}

Bin::Bin(int x, int y, int lx, int ly, int ux, int uy, float targetDensity)
    : Bin()
{
  index_.set(x, y);
  rect_.reset(lx, ly, ux, uy);
  setTargetDensity(targetDensity);
}

Bin::~Bin()
{
}

void Bin::setTargetDensity(float density)
{
  targetDensity_ = density;
  targetDensityArea_ = binArea() * targetDensity_;
}

void Bin::updateDensity()
{
  density_ = static_cast<float>(densityArea()) / targetDensityArea();
}

int64_t Bin::densityArea() const
{
  return nonPlaceArea_ + instPlacedArea_ + fillerArea_;
}

float Bin::overflowArea() const
{
  const float binTargetArea = targetDensityArea();
  const float occupiedNonFillerArea = instPlacedArea_ + nonPlaceArea_;
  const float overflow = occupiedNonFillerArea - binTargetArea;
#if 0
  if (overflow > 0) {
    std::cout << "  Eden Bin(" << x() << "," << y()
              << "): binTargetArea=" << binTargetArea
              << ", instPlacedArea_=" << instPlacedArea_
              << ", nonPlaceArea_=" << nonPlaceArea_
              << ", occupiedNonFillerArea=" << occupiedNonFillerArea
              << ", overflow=" << overflow 
              << ", binArea=" << binArea()
              << std::endl;
  }
#endif
  return std::max(0.0f, overflow);
}

void Bin::resetNonPlaceArea()
{
  nonPlaceArea_ = 0;
}

// Note that nonPlaceArea should have scale-down with
// target density.
// See MS-replace paper
//
void Bin::addNonPlaceArea(int64_t area)
{
  nonPlaceArea_ += area * targetDensity();
}

void Bin::resetPlaceArea()
{
  instPlacedArea_ = 0;
  fillerArea_ = 0;
}

void Bin::addPlaceArea(const GCell* gcell)
{
  auto area = getOverlapDensityArea(this, gcell);
  if (gcell->isMacroInstance()) {
    // macro should have scale-down with target-density
    area *= targetDensity();
    instPlacedArea_ += static_cast<int64_t>(area);
  } else if (gcell->isStdInstance()) {
    instPlacedArea_ += static_cast<int64_t>(area);
  } else {
    assert(gcell->isFiller());
    fillerArea_ += static_cast<int64_t>(area);
  }
}

////////////////////////////////////////////////
// BinGrid

BinGrid::BinGrid()
    : IntRect(),
      binCntX_(0),
      binCntY_(0),
      binSizeX_(0),
      binSizeY_(0),
      targetDensity_(0),
      overflowArea_(0),
      isSetBinCntX_(0),
      isSetBinCntY_(0)
{
}

BinGrid::BinGrid(Die* die) : BinGrid()
{
  setCorePoints(die);
}

BinGrid::~BinGrid()
{
  binStor_.clear();
  bins_.clear();
}

void BinGrid::setCorePoints(const Die* die)
{
  rect_.reset(die->coreLx(), die->coreLy(), die->coreUx(), die->coreUy());
}

void BinGrid::setPlacerBase(const std::shared_ptr<PlacerBase> pb)
{
  pb_ = pb;
}

void BinGrid::setLogger(utl::Logger* log)
{
  log_ = log;
}

void BinGrid::setTargetDensity(float density)
{
  targetDensity_ = density;
}

void BinGrid::setBinCnt(int binCntX, int binCntY)
{
  setBinCntX(binCntX);
  setBinCntY(binCntY);
}

void BinGrid::setBinCntX(int binCntX)
{
  isSetBinCntX_ = 1;
  binCntX_ = binCntX;
}

void BinGrid::setBinCntY(int binCntY)
{
  isSetBinCntY_ = 1;
  binCntY_ = binCntY;
}

void BinGrid::initBins()
{
  int64_t totalBinArea = rect_.area();

  int64_t averagePlaceInstArea
      = pb_->placeInstsArea() / pb_->placeInsts().size();

  int64_t idealBinArea
      = std::round(static_cast<float>(averagePlaceInstArea) / targetDensity_);
  int idealBinCnt = totalBinArea / idealBinArea;
  if (idealBinCnt < 4) {  // the smallest we allow is 2x2 bins
    idealBinCnt = 4;
  }

  log_->info(GPL, 23, "TargetDensity: {:.2f}", targetDensity_);
  log_->info(GPL, 24, "AveragePlaceInstArea: {}", averagePlaceInstArea);
  log_->info(GPL, 25, "IdealBinArea: {}", idealBinArea);
  log_->info(GPL, 26, "IdealBinCnt: {}", idealBinCnt);
  log_->info(GPL, 27, "TotalBinArea: {}", totalBinArea);

  int foundBinCnt = 2;
  // find binCnt: 2, 4, 8, 16, 32, 64, ...
  // s.t. binCnt^2 <= idealBinCnt <= (binCnt*2)^2.
  for (foundBinCnt = 2; foundBinCnt <= 1024; foundBinCnt *= 2) {
    if (foundBinCnt * foundBinCnt <= idealBinCnt
        && 4 * foundBinCnt * foundBinCnt > idealBinCnt) {
      break;
    }
  }

  // setBinCntX_;
  if (!isSetBinCntX_) {
    binCntX_ = foundBinCnt;
  }

  // setBinCntY_;
  if (!isSetBinCntY_) {
    binCntY_ = foundBinCnt;
  }

  log_->info(GPL, 28, "BinCnt: {} {}", binCntX_, binCntY_);

  binSizeX_ = ceil(static_cast<float>(dx()) / binCntX_);
  binSizeY_ = ceil(static_cast<float>(dy()) / binCntY_);

  log_->info(GPL, 29, "BinSize: {} {}", binSizeX_, binSizeY_);

  // initialize binStor_, bins_ vector
  size_t binsCnt = binCntX_ * binCntY_;
  binStor_.reserve(binsCnt);
  bins_.reserve(binsCnt);
  int x = lx(), y = ly();
  int idxX = 0, idxY = 0;
  for (size_t i = 0; i < binsCnt; ++i) {
    int sizeX = (x + binSizeX_ > ux()) ? ux() - x : binSizeX_;
    int sizeY = (y + binSizeY_ > uy()) ? uy() - y : binSizeY_;

    // cout << "idxX: " << idxX << " idxY: " << idxY
    //  << " x:" << x << " y:" << y
    //  << " " << x+sizeX << " " << y+sizeY << endl;
    binStor_.emplace_back(
        Bin(idxX, idxY, x, y, x + sizeX, y + sizeY, targetDensity_));

    // move x, y coordinates.
    x += binSizeX_;
    idxX += 1;

    if (x >= ux()) {
      y += binSizeY_;
      x = lx();

      idxY++;
      idxX = 0;
    }

    bins_.push_back(&*binStor_.rbegin());
  }

  log_->info(GPL, 30, "NumBins: {}", bins_.size());

  // only initialized once
  updateBinsNonPlaceArea();
}

void BinGrid::updateBinsNonPlaceArea()
{
  for (auto& bin : bins_) {
    bin->resetNonPlaceArea();
  }

  for (auto& inst : pb_->nonPlaceInsts()) {
    std::pair<int, int> pairX = getMinMaxIdxX(inst);
    std::pair<int, int> pairY = getMinMaxIdxY(inst);
    for (int i = pairX.first; i < pairX.second; i++) {
      for (int j = pairY.first; j < pairY.second; j++) {
        Bin* bin = binAt(i, j);

        bin->addNonPlaceArea(getOverlapArea(bin, inst));
      }
    }
  }
}

// Core Part
void BinGrid::updateBinsGCellDensityArea(const std::vector<GCell*>& cells)
{
  // clear the Bin-area info
  for (auto& bin : bins_) {
    bin->resetPlaceArea();
  }

  for (auto& cell : cells) {
    std::pair<int, int> pairX = getDensityMinMaxIdxX(cell);
    std::pair<int, int> pairY = getDensityMinMaxIdxY(cell);

    // The following function is critical runtime hotspot
    // for global placer.
    //
    for (int i = pairX.first; i < pairX.second; i++) {
      for (int j = pairY.first; j < pairY.second; j++) {
        Bin* bin = binAt(i, j);
        bin->addPlaceArea(cell);
      }
    }
  }

  overflowArea_ = 0;

  // update density and overflowArea
  // for nesterov use and FFT library
  for (auto& bin : bins_) {
    bin->updateDensity();
    overflowArea_ += bin->overflowArea();
  }
}

static int lowerBinIdx(int distance, int size)
{
  return distance / size;
}

static int upperBinIdx(int distance, int size)
{
  return (distance + size - 1) / size;
}

std::pair<int, int> BinGrid::getDensityMinMaxIdxX(const GCell* gcell) const
{
  int lowerIdx = lowerBinIdx(gcell->dLx() - lx(), binSizeX_);
  int upperIdx = upperBinIdx(gcell->dUx() - lx(), binSizeX_);
  return std::make_pair(lowerIdx, upperIdx);
}

std::pair<int, int> BinGrid::getDensityMinMaxIdxY(const GCell* gcell) const
{
  int lowerIdx = lowerBinIdx(gcell->dLy() - ly(), binSizeY_);
  int upperIdx = upperBinIdx(gcell->dUy() - ly(), binSizeY_);
  return std::make_pair(lowerIdx, upperIdx);
}

std::pair<int, int> BinGrid::getMinMaxIdxX(const Instance* inst) const
{
  int lowerIdx = lowerBinIdx(inst->lx() - lx(), binSizeX_);
  int upperIdx = upperBinIdx(inst->ux() - lx(), binSizeX_);
  return std::make_pair(std::max(lowerIdx, 0), std::min(upperIdx, binCntX_));
}

std::pair<int, int> BinGrid::getMinMaxIdxY(const Instance* inst) const
{
  int lowerIdx = lowerBinIdx(inst->ly() - ly(), binSizeY_);
  int upperIdx = upperBinIdx(inst->uy() - ly(), binSizeY_);
  return std::make_pair(std::max(lowerIdx, 0), std::min(upperIdx, binCntY_));
}

std::pair<int, int> BinGrid::getBinCenterCoord(int xIdx, int yIdx) const
{
  int x = lx() + (xIdx * binSizeX_ + binSizeX_ / 2);
  int y = ly() + (yIdx * binSizeY_ + binSizeY_ / 2);
  if (xIdx < 0) {
    x = lx();
  } else if (xIdx >= binCntX_) {
    x = ux();
  }

  if (yIdx < 0) {
    y = ly();
  } else if (yIdx >= binCntY_) {
    y = uy();
  }
  return std::make_pair(x, y);
}

IntRect BinGrid::getDensityFieldInterpolationBox(const GCell* gcell) const
{
  int lx_ = gcell->dCx() - binSizeX_ / 2;
  int ly_ = gcell->dCy() - binSizeY_ / 2;
  int x0 = lx_ > lx() ? lowerBinIdx(lx_ - lx(), binSizeX_) : -1;
  int y0 = ly_ > ly() ? lowerBinIdx(ly_ - ly(), binSizeY_) : -1;
  int x1 = x0 + 1;
  int y1 = y0 + 1;
  return IntRect(x0, y0, x1, y1);
}

////////////////////////////////////////////////
// NesterovBaseVars
NesterovBaseVars::NesterovBaseVars()
{
  reset();
}

void NesterovBaseVars::reset()
{
  targetDensity = 1.0;
  binCntX = binCntY = 0;
  minWireLengthForceBar = -300;
  cellDensityForceCalPolicy = "";
  isSetBinCntX = isSetBinCntY = 0;
  useUniformTargetDensity = 0;
}

////////////////////////////////////////////////
// NesterovBase

NesterovBase::NesterovBase()
    : pb_(nullptr),
      log_(nullptr),
      fillerDx_(0),
      fillerDy_(0),
      whiteSpaceArea_(0),
      movableArea_(0),
      totalFillerArea_(0),
      stdInstsArea_(0),
      macroInstsArea_(0),
      sumPhi_(0),
      targetDensity_(0),
      uniformTargetDensity_(0)
{
}

NesterovBase::NesterovBase(NesterovBaseVars nbVars,
                           std::shared_ptr<PlacerBase> pb,
                           utl::Logger* log)
    : NesterovBase()
{
  nbVars_ = nbVars;
  pb_ = pb;
  log_ = log;
  init();
}

NesterovBase::~NesterovBase()
{
  reset();
}

void NesterovBase::reset()
{
  nbVars_.reset();
  pb_ = nullptr;
  log_ = nullptr;

  fillerDx_ = fillerDy_ = 0;
  whiteSpaceArea_ = movableArea_ = 0;
  totalFillerArea_ = 0;

  stdInstsArea_ = macroInstsArea_ = 0;

  gCellStor_.clear();
  gNetStor_.clear();
  gPinStor_.clear();

  gCells_.clear();
  gCellInsts_.clear();
  gCellFillers_.clear();

  gNets_.clear();
  gPins_.clear();

  gCellMap_.clear();
  gPinMap_.clear();
  gNetMap_.clear();

  gCellStor_.shrink_to_fit();
  gNetStor_.shrink_to_fit();
  gPinStor_.shrink_to_fit();

  gCells_.shrink_to_fit();
  gCellInsts_.shrink_to_fit();
  gCellFillers_.shrink_to_fit();

  gNets_.shrink_to_fit();
  gPins_.shrink_to_fit();

  sumPhi_ = 0;
  targetDensity_ = 0;
  uniformTargetDensity_ = 0;
}

void NesterovBase::init()
{
  // area update from pb
  stdInstsArea_ = pb_->stdInstsArea();
  macroInstsArea_ = pb_->macroInstsArea();

  // gCellStor init
  gCellStor_.reserve(pb_->placeInsts().size());
  for (auto& inst : pb_->placeInsts()) {
    gCellStor_.push_back(GCell(inst));
  }

  // TODO:
  // at this moment, GNet and GPin is equal to
  // Net and Pin

  // gPinStor init
  gPinStor_.reserve(pb_->pins().size());
  for (auto& pin : pb_->pins()) {
    GPin myGPin(pin);
    gPinStor_.push_back(myGPin);
  }

  // gNetStor init
  gNetStor_.reserve(pb_->nets().size());
  for (auto& net : pb_->nets()) {
    GNet myGNet(net);
    gNetStor_.push_back(myGNet);
  }

  // update gFillerCells
  initFillerGCells();

  // gCell ptr init
  gCells_.reserve(gCellStor_.size());
  for (auto& gCell : gCellStor_) {
    gCells_.push_back(&gCell);
    if (gCell.isInstance()) {
      gCellMap_[gCell.instance()] = &gCell;
      gCellInsts_.push_back(&gCell);
    } else if (gCell.isFiller()) {
      gCellFillers_.push_back(&gCell);
    }
  }

  // gPin ptr init
  gPins_.reserve(gPinStor_.size());
  for (auto& gPin : gPinStor_) {
    gPins_.push_back(&gPin);
    gPinMap_[gPin.pin()] = &gPin;
  }

  // gNet ptr init
  gNets_.reserve(gNetStor_.size());
  for (auto& gNet : gNetStor_) {
    gNets_.push_back(&gNet);
    gNetMap_[gNet.net()] = &gNet;
  }

  // gCellStor_'s pins_ fill
  for (auto& gCell : gCellStor_) {
    if (gCell.isFiller()) {
      continue;
    }

    for (auto& pin : gCell.instance()->pins()) {
      gCell.addGPin(pbToNb(pin));
    }
  }

  // gPinStor_' GNet and GCell fill
  for (auto& gPin : gPinStor_) {
    gPin.setGCell(pbToNb(gPin.pin()->instance()));
    gPin.setGNet(pbToNb(gPin.pin()->net()));
  }

  // gNetStor_'s GPin fill
  for (auto& gNet : gNetStor_) {
    for (auto& pin : gNet.net()->pins()) {
      gNet.addGPin(pbToNb(pin));
    }
  }

  log_->info(GPL, 31, "FillerInit: NumGCells: {}", gCells_.size());
  log_->info(GPL, 32, "FillerInit: NumGNets: {}", gNets_.size());
  log_->info(GPL, 33, "FillerInit: NumGPins: {}", gPins_.size());

  // initialize bin grid structure
  // send param into binGrid structure
  if (nbVars_.isSetBinCntX) {
    bg_.setBinCntX(nbVars_.binCntX);
  }

  if (nbVars_.isSetBinCntY) {
    bg_.setBinCntY(nbVars_.binCntY);
  }

  bg_.setPlacerBase(pb_);
  bg_.setLogger(log_);
  bg_.setCorePoints(&(pb_->die()));
  bg_.setTargetDensity(targetDensity_);

  // update binGrid info
  bg_.initBins();

  // initialize fft structrue based on bins
  std::unique_ptr<FFT> fft(
      new FFT(bg_.binCntX(), bg_.binCntY(), bg_.binSizeX(), bg_.binSizeY()));

  fft_ = std::move(fft);

  // update densitySize and densityScale in each gCell
  updateDensitySize();
}

// virtual filler GCells
void NesterovBase::initFillerGCells()
{
  // extract average dx/dy in range (10%, 90%)
  vector<int> dxStor;
  vector<int> dyStor;

  dxStor.reserve(pb_->placeInsts().size());
  dyStor.reserve(pb_->placeInsts().size());
  for (auto& placeInst : pb_->placeInsts()) {
    dxStor.push_back(placeInst->dx());
    dyStor.push_back(placeInst->dy());
  }

  // sort
  std::sort(dxStor.begin(), dxStor.end());
  std::sort(dyStor.begin(), dyStor.end());

  // average from (10 - 90%) .
  int64_t dxSum = 0, dySum = 0;

  int minIdx = dxStor.size() * 0.05;
  int maxIdx = dxStor.size() * 0.95;

  // when #instances are too small,
  // extracts average values in whole ranges.
  if (minIdx == maxIdx) {
    minIdx = 0;
    maxIdx = dxStor.size();
  }

  for (int i = minIdx; i < maxIdx; i++) {
    dxSum += dxStor[i];
    dySum += dyStor[i];
  }

  // the avgDx and avgDy will be used as filler cells'
  // width and height
  fillerDx_ = static_cast<int>(dxSum / (maxIdx - minIdx));
  fillerDy_ = static_cast<int>(dySum / (maxIdx - minIdx));

  int64_t coreArea = pb_->die().coreArea();

  // nonPlaceInstsArea should not have density downscaling!!!
  whiteSpaceArea_ = coreArea - pb_->nonPlaceInstsArea();

  // targetDensity initialize
  if (nbVars_.useUniformTargetDensity) {
    targetDensity_ = static_cast<float>(stdInstsArea_)
                         / static_cast<float>(whiteSpaceArea_ - macroInstsArea_)
                     + 0.01;
  } else {
    targetDensity_ = nbVars_.targetDensity;
  }

  // TODO density screening
  movableArea_ = whiteSpaceArea_ * targetDensity_;

  totalFillerArea_ = movableArea_ - nesterovInstsArea();
  uniformTargetDensity_ = static_cast<float>(nesterovInstsArea())
                          / static_cast<float>(whiteSpaceArea_);
  if (totalFillerArea_ < 0) {
    log_->error(GPL,
                302,
                "Use a higher -density or "
                "re-floorplan with a larger core area.\n"
                "Given target density: {:.2f}\n"
                "Suggested target density: {:.2f}",
                targetDensity_,
                uniformTargetDensity_);
  }

  if (nbVars_.disableFillerInsertion) {
    debugPrint(log_,
               GPL,
               "replace",
               1,
               "FillerInit is disabled by option -disable_filler_insertion");
    totalFillerArea_ = 0;
  }

  int fillerCnt = static_cast<int>(
      totalFillerArea_ / static_cast<int64_t>(fillerDx_ * fillerDy_));

  debugPrint(log_, GPL, "replace", 3, "FillerInit: CoreArea {}", coreArea);
  debugPrint(log_,
             GPL,
             "replace",
             3,
             "FillerInit: WhiteSpaceArea {}",
             whiteSpaceArea_);
  debugPrint(
      log_, GPL, "replace", 3, "FillerInit: MovableArea {}", movableArea_);
  debugPrint(log_,
             GPL,
             "replace",
             3,
             "FillerInit: TotalFillerArea {}",
             totalFillerArea_);
  debugPrint(
      log_, GPL, "replace", 3, "FillerInit: NumFillerCells {}", fillerCnt);
  debugPrint(log_,
             GPL,
             "replace",
             3,
             "FillerInit: FillerCellArea {}",
             fillerCellArea());
  debugPrint(log_,
             GPL,
             "replace",
             3,
             "FillerInit: FillerCellSize {} {}",
             fillerDx_,
             fillerDy_);

  //
  // mt19937 supports huge range of random values.
  // rand()'s RAND_MAX is only 32767.
  //
  mt19937 randVal(0);
  gCellStor_.reserve(gCellStor_.size() + fillerCnt);
  for (int i = 0; i < fillerCnt; i++) {
    // instability problem between g++ and clang++!
    auto randX = randVal();
    auto randY = randVal();

    // place filler cells on random coordi and
    // set size as avgDx and avgDy
    odb::Point center(randX % pb_->die().coreDx() + pb_->die().coreLx(),
                      randY % pb_->die().coreDy() + pb_->die().coreLy());

    gCellStor_.emplace_back(center, fillerDx_, fillerDy_);
  }
}

GCell* NesterovBase::pbToNb(Instance* inst) const
{
  auto gcPtr = gCellMap_.find(inst);
  return (gcPtr == gCellMap_.end()) ? nullptr : gcPtr->second;
}

GPin* NesterovBase::pbToNb(Pin* pin) const
{
  auto gpPtr = gPinMap_.find(pin);
  return (gpPtr == gPinMap_.end()) ? nullptr : gpPtr->second;
}

GNet* NesterovBase::pbToNb(Net* net) const
{
  auto gnPtr = gNetMap_.find(net);
  return (gnPtr == gNetMap_.end()) ? nullptr : gnPtr->second;
}

GCell* NesterovBase::dbToNb(odb::dbInst* inst) const
{
  Instance* pbInst = pb_->dbToPb(inst);
  return pbToNb(pbInst);
}

GPin* NesterovBase::dbToNb(odb::dbITerm* pin) const
{
  Pin* pbPin = pb_->dbToPb(pin);
  return pbToNb(pbPin);
}

GPin* NesterovBase::dbToNb(odb::dbBTerm* pin) const
{
  Pin* pbPin = pb_->dbToPb(pin);
  return pbToNb(pbPin);
}

GNet* NesterovBase::dbToNb(odb::dbNet* net) const
{
  Net* pbNet = pb_->dbToPb(net);
  return pbToNb(pbNet);
}

void NesterovBase::updateGCellDensityCenterLocation(
    const std::vector<FloatPoint>& coordis)
{
  for (size_t idx = 0; idx < coordis.size(); ++idx) {
    auto coordi = coordis[idx];
    gCells_[idx]->setDensityCenterLocation(coordi.x, coordi.y);
  }
  bg_.updateBinsGCellDensityArea(gCells_);
}

void NesterovBase::setTargetDensity(float density)
{
  targetDensity_ = density;
  bg_.setTargetDensity(density);
  for (auto& bin : bins()) {
    bin->setTargetDensity(density);
  }
  // update nonPlaceArea's target denstiy
  bg_.updateBinsNonPlaceArea();
}

int NesterovBase::binCntX() const
{
  return bg_.binCntX();
}

int NesterovBase::binCntY() const
{
  return bg_.binCntY();
}

int NesterovBase::binSizeX() const
{
  return bg_.binSizeX();
}

int NesterovBase::binSizeY() const
{
  return bg_.binSizeY();
}

int64_t NesterovBase::overflowArea() const
{
  return bg_.overflowArea();
}

float NesterovBase::densityOverflow() const
{
  return static_cast<float>(overflowArea())
         / static_cast<float>(nesterovInstsArea());
}

int NesterovBase::fillerDx() const
{
  return fillerDx_;
}

int NesterovBase::fillerDy() const
{
  return fillerDy_;
}

int NesterovBase::fillerCnt() const
{
  return static_cast<int>(gCellFillers_.size());
}

int64_t NesterovBase::fillerCellArea() const
{
  return static_cast<int64_t>(fillerDx_) * static_cast<int64_t>(fillerDy_);
}

int64_t NesterovBase::whiteSpaceArea() const
{
  return whiteSpaceArea_;
}

int64_t NesterovBase::movableArea() const
{
  return movableArea_;
}

int64_t NesterovBase::totalFillerArea() const
{
  return totalFillerArea_;
}

int64_t NesterovBase::nesterovInstsArea() const
{
  return stdInstsArea_
         + static_cast<int64_t>(round(macroInstsArea_ * targetDensity_));
}

float NesterovBase::sumPhi() const
{
  return sumPhi_;
}

float NesterovBase::uniformTargetDensity() const
{
  return uniformTargetDensity_;
}

float NesterovBase::initTargetDensity() const
{
  return nbVars_.targetDensity;
}

float NesterovBase::targetDensity() const
{
  return targetDensity_;
}

// update densitySize and densityScale in each gCell
void NesterovBase::updateDensitySize()
{
  if (nbVars_.disableCellDensityScaling) {
    debugPrint(log_,
               GPL,
               "replace",
               1,
               "cell density scaling is disabled by option "
               "-disable_cell_density_scaling");
    return;
  }

  for (auto& gCell : gCells_) {
    float densitySizeX = REPLACE_SQRT2 * bg_.binSizeX();
    float densitySizeY = REPLACE_SQRT2 * bg_.binSizeY();
    float scaleX = gCell->dx() / densitySizeX;
    float scaleY = gCell->dy() / densitySizeY;
    if (gCell->dx() > densitySizeX) {
      scaleX = 1.0;
      densitySizeX = gCell->dx();
    }

    if (gCell->dy() > densitySizeY) {
      scaleY = 1.0;
      densitySizeY = gCell->dy();
    }

    gCell->setDensitySize(densitySizeX, densitySizeY);
    gCell->setDensityScale(scaleX * scaleY);
  }
}

void NesterovBase::updateAreas()
{
  // bloating can change the following :
  // stdInstsArea and macroInstsArea
  stdInstsArea_ = macroInstsArea_ = 0;
  for (auto* gCell : gCells_) {
    if (!gCell->isInstance()) {
      continue;
    }
    const float r = static_cast<float>(gCell->dx())
                    / static_cast<float>(gCell->instance()->dx());
    const float area_inflation = r * r;
    auto const inst_area = gCell->instance()->area() * area_inflation;
    if (gCell->isMacroInstance()) {
      macroInstsArea_ += inst_area;
    } else if (gCell->isStdInstance()) {
      stdInstsArea_ += inst_area;
    }
  }

  int64_t coreArea = pb_->die().coreArea();

  // nonPlaceInstsArea should not have density downscaling!!!
  whiteSpaceArea_ = coreArea - static_cast<int64_t>(pb_->nonPlaceInstsArea());

  // TODO density screening
  movableArea_ = whiteSpaceArea_ * targetDensity_;

  totalFillerArea_ = movableArea_ - nesterovInstsArea();
  uniformTargetDensity_ = static_cast<float>(nesterovInstsArea())
                          / static_cast<float>(whiteSpaceArea_);

  if (totalFillerArea_ < 0) {
    log_->error(GPL,
                303,
                "Use a higher -density or "
                "re-floorplan with a larger core area.\n"
                "Given target density: {:.2f}\n"
                "Suggested target density: {:.2f}",
                targetDensity_,
                uniformTargetDensity_);
  }
}

// cut the filler cells
void NesterovBase::cutFillerCells(int64_t targetFillerArea)
{
  std::vector<GCell*> newGCells = gCellInsts_;
  std::vector<GCell*> newGCellFillers;

  int64_t curFillerArea = 0;
  log_->info(GPL, 34, "gCellFiller: {}", gCellFillers_.size());

  for (auto& gCellFiller : gCellFillers_) {
    curFillerArea += static_cast<int64_t>(gCellFiller->dx())
                     * static_cast<int64_t>(gCellFiller->dy());

    if (curFillerArea >= targetFillerArea) {
      curFillerArea -= static_cast<int64_t>(gCellFiller->dx())
                       * static_cast<int64_t>(gCellFiller->dy());
      break;
    }

    newGCells.push_back(gCellFiller);
    newGCellFillers.push_back(gCellFiller);
  }

  // update totalFillerArea_
  totalFillerArea_ = curFillerArea;
  log_->info(GPL, 35, "NewTotalFillerArea: {}", totalFillerArea_);

  gCells_.swap(newGCells);
  gCellFillers_.swap(newGCellFillers);
}

void NesterovBase::updateDensityCoordiLayoutInside(GCell* gCell)
{
  float targetLx
      = util::bounded_value(gCell->dLx(), bg_.lx(), bg_.ux() - gCell->dDx());
  float targetLy
      = util::bounded_value(gCell->dLy(), bg_.ly(), bg_.uy() - gCell->dDy());
  gCell->setDensityLocation(targetLx, targetLy);
}

float NesterovBase::getDensityCoordiLayoutInsideX(const GCell* gCell,
                                                  float cx) const
{
  float adjVal = cx;
  // TODO will change base on each assigned binGrids.
  //
  float min = bg_.lx() + gCell->dDx() / 2;
  float max = bg_.ux() - gCell->dDx() / 2;
  return util::bounded_value(adjVal, min, max);
}

float NesterovBase::getDensityCoordiLayoutInsideY(const GCell* gCell,
                                                  float cy) const
{
  float adjVal = cy;
  // TODO will change base on each assigned binGrids.
  //
  float min = bg_.ly() + gCell->dDy() / 2;
  float max = bg_.uy() - gCell->dDy() / 2;
  return util::bounded_value(adjVal, min, max);
}

//
// WA force cals - wlCoeffX / wlCoeffY
//
// * Note that wlCoeffX and wlCoeffY is 1/gamma
// in ePlace paper.
void NesterovBase::updateWireLengthForceWA(float wlCoeffX, float wlCoeffY)
{
  // clear all WA variables.
  for (auto& gNet : gNets_) {
    gNet->clearWaVars();
  }
  for (auto& gPin : gPins_) {
    gPin->clearWaVars();
  }

  for (auto& gNet : gNets_) {
    gNet->updateBox();
#if 0  // debug
    if (gNet->net()->dbNet()->getName() == "HCLK") {
      std::cout << "Eden net: {name: " << gNet->net()->dbNet()->getName() << "}"
                <<", bbox: Rect{"
                    << "ll: (" << gNet->lx() << ", "
                    << gNet->ly() << ")"
                    << "ur: (" << gNet->ux() << ", "
                    << gNet->uy() << ")"
                    << "}"
                << "}"
                << std::endl;
      std::cout << "Eden wlCoeffX: " << wlCoeffX << std::endl;
      std::cout << "Eden wlCoeffY: " << wlCoeffY << std::endl;
    }
#endif
    for (auto& gPin : gNet->gPins()) {
      // The WA terms are shift invariant:
      //
      //   Sum(x_i * exp(x_i))    Sum(x_i * exp(x_i - C))
      //   -----------------    = -----------------
      //   Sum(exp(x_i))          Sum(exp(x_i - C))
      //
      // So we shift to keep the exponential from overflowing
      float expMinX = (gNet->lx() - gPin->cx()) * wlCoeffX;
      float expMaxX = (gPin->cx() - gNet->ux()) * wlCoeffX;
      float expMinY = (gNet->ly() - gPin->cy()) * wlCoeffY;
      float expMaxY = (gPin->cy() - gNet->uy()) * wlCoeffY;

#if 0
      if (gNet->net()->dbNet()->getName() == "HCLK") {
        auto pin = *gPin->pin();
        auto cell = gPin->gCell();

        std::cout << "Eden Pin{odb_term:"
                  << (pin.isBTerm() ? (uintptr_t) pin.dbBTerm()
                                    : (uintptr_t) pin.dbITerm())
                  << ", term_type:"
                  << (pin.isBTerm() ? "ODB_BTERM" : "ODB_ITERM")
                  << ", name:" << pin.name();
        if (cell) {
          auto cell_rect = cell->rect();
          std::cout << ", cell: Cell{rect: Rect{"
                    << "ll: (" << cell_rect.ll().getX() << ", "
                    << cell_rect.ll().getY() << ")"
                    << "ur: (" << cell_rect.ur().getX() << ", "
                    << cell_rect.ur().getY() << ")"
                    << "}"
                    << ", pins.size(): " << cell->gPins().size()
                    << ", odb_instance: " << cell->instance()->dbInst()
                    << ", inst_area: " << cell->instance()->area()
                    << ", name: " << cell->instance()->dbInst()->getName()
                    << "}";

        } else {
          std::cout << ", cell: nullptr";
        }
        std::cout << ", net: Net{odb_net:" << (uintptr_t) gNet->net()->dbNet()
                  << ", name:" << gNet->net()->dbNet()->getName()
                  << ", pins.size(): " << gNet->gPins().size() << "}"
                  << ", center: (" << gPin->cx() << "," << gPin->cy()
                  << "), offset: (" << gPin->offsetCx() << ","
                  << gPin->offsetCy() << ")}" << std::endl;

        std::cout << "expMinX:" << expMinX
                  << "fastExp(expMinX): " << fastExp(expMinX) << std::endl;
        std::cout << "expMaxX:" << expMaxX
                  << "fastExp(expMaxX): " << fastExp(expMaxX) << std::endl;
        std::cout << "expMinY:" << expMinY
                  << "fastExp(expMinY): " << fastExp(expMinY) << std::endl;
        std::cout << "expMaxY:" << expMaxY
                  << "fastExp(expMaxY): " << fastExp(expMaxY) << std::endl;
        std::cout << std::endl;
      }
#endif
      // min x
      if (expMinX > nbVars_.minWireLengthForceBar) {
        gPin->setMinExpSumX(fastExp(expMinX));
#if 0
        if (gPin->minExpSumX() == 0) {
          std::cout << "(gPin->minExpSumX()):" << (gPin->minExpSumX())
                    << "(expMinX):" << (expMinX)
                    << "(fastExp(expMinX)):" << (fastExp(expMinX)) << std::endl;
          assert(gPin->minExpSumX() != 0);
        }
#endif

        gNet->addWaExpMinSumX(gPin->minExpSumX());
        gNet->addWaXExpMinSumX(gPin->cx() * gPin->minExpSumX());
        if (gPin->gCell() && gPin->gCell()->isInstance()) {
          debugPrint(log_,
                     GPL,
                     "replace",
                     5,
                     "wlUpdateWA:  MinX updated: {} {:g}",
                     gPin->gCell()->instance()->dbInst()->getConstName(),
                     gPin->minExpSumX());
        }
      }

      // max x
      if (expMaxX > nbVars_.minWireLengthForceBar) {
        gPin->setMaxExpSumX(fastExp(expMaxX));
        gNet->addWaExpMaxSumX(gPin->maxExpSumX());
        gNet->addWaXExpMaxSumX(gPin->cx() * gPin->maxExpSumX());
        if (gPin->gCell() && gPin->gCell()->isInstance()) {
          debugPrint(log_,
                     GPL,
                     "replace",
                     5,
                     "wlUpdateWA:  MaxX updated: {} {:g}",
                     gPin->gCell()->instance()->dbInst()->getConstName(),
                     gPin->maxExpSumX());
        }
      }

      // min y
      if (expMinY > nbVars_.minWireLengthForceBar) {
        gPin->setMinExpSumY(fastExp(expMinY));
        gNet->addWaExpMinSumY(gPin->minExpSumY());
        gNet->addWaYExpMinSumY(gPin->cy() * gPin->minExpSumY());
        if (gPin->gCell() && gPin->gCell()->isInstance()) {
          debugPrint(log_,
                     GPL,
                     "replace",
                     5,
                     "wlUpdateWA:  MinY updated: {} {:g}",
                     gPin->gCell()->instance()->dbInst()->getConstName(),
                     gPin->minExpSumY());
        }
      }

      // max y
      if (expMaxY > nbVars_.minWireLengthForceBar) {
        gPin->setMaxExpSumY(fastExp(expMaxY));
        gNet->addWaExpMaxSumY(gPin->maxExpSumY());
        gNet->addWaYExpMaxSumY(gPin->cy() * gPin->maxExpSumY());
        if (gPin->gCell() && gPin->gCell()->isInstance()) {
          debugPrint(log_,
                     GPL,
                     "replace",
                     5,
                     "wlUpdateWA:  MaxY updated: {} {:g}",
                     gPin->gCell()->instance()->dbInst()->getConstName(),
                     gPin->maxExpSumY());
        }
      }
    }
#if 0  // debug
    if (gNet->net()->dbNet()->getName() == "HCLK") {
      std::cout << "netname: " << gNet->net()->dbNet()->getName()
                << "gNet->waExpMinSumX()" << gNet->waExpMinSumX()
                << "gNet->waExpMinSumY()" << gNet->waExpMinSumY() << std::endl;
    }
#endif
    assert(gNet->waExpMinSumX() != 0);
    assert(gNet->waExpMinSumY() != 0);

    // cout << gNet->lx() << " " << gNet->ly() << " "
    //  << gNet->ux() << " " << gNet->uy() << endl;
  }
}

// get x,y WA Gradient values with given GCell
FloatPoint NesterovBase::getWireLengthGradientWA(const GCell* gCell,
                                                 float wlCoeffX,
                                                 float wlCoeffY) const
{
  FloatPoint gradientPair;

  for (auto& gPin : gCell->gPins()) {
    auto tmpPair = getWireLengthGradientPinWA(gPin, wlCoeffX, wlCoeffY);

    debugPrint(log_,
               GPL,
               "replace",
               5,
               "getGradientWA: wlPair: {:g} {:g}",
               tmpPair.x,
               tmpPair.y);

    // apply timing/custom net weight
    tmpPair.x *= gPin->gNet()->totalWeight();
    tmpPair.y *= gPin->gNet()->totalWeight();

    gradientPair.x += tmpPair.x;
    gradientPair.y += tmpPair.y;
  }

  if (gCell->isInstance()) {
    debugPrint(log_,
               GPL,
               "replace",
               5,
               "getGradientWA:  gradient: {:g} {:g}",
               gradientPair.x,
               gradientPair.y);
  }

  // return sum
  return gradientPair;
}

// get x,y WA Gradient values from GPin
// Please check the JingWei's Ph.D. thesis full paper,
// Equation (4.13)
//
// You can't understand the following function
// unless you read the (4.13) formula
FloatPoint NesterovBase::getWireLengthGradientPinWA(const GPin* gPin,
                                                    float wlCoeffX,
                                                    float wlCoeffY) const
{
  float gradientMinX = 0, gradientMinY = 0;
  float gradientMaxX = 0, gradientMaxY = 0;

  // min x
  if (gPin->hasMinExpSumX()) {
    // from Net.
    float waExpMinSumX = gPin->gNet()->waExpMinSumX();
    float waXExpMinSumX = gPin->gNet()->waXExpMinSumX();

    gradientMinX
        = (waExpMinSumX * (gPin->minExpSumX() * (1.0 - wlCoeffX * gPin->cx()))
           + wlCoeffX * gPin->minExpSumX() * waXExpMinSumX)
          / (waExpMinSumX * waExpMinSumX);
  }

  // max x
  if (gPin->hasMaxExpSumX()) {
    float waExpMaxSumX = gPin->gNet()->waExpMaxSumX();
    float waXExpMaxSumX = gPin->gNet()->waXExpMaxSumX();

    gradientMaxX
        = (waExpMaxSumX * (gPin->maxExpSumX() * (1.0 + wlCoeffX * gPin->cx()))
           - wlCoeffX * gPin->maxExpSumX() * waXExpMaxSumX)
          / (waExpMaxSumX * waExpMaxSumX);
  }

  // min y
  if (gPin->hasMinExpSumY()) {
    float waExpMinSumY = gPin->gNet()->waExpMinSumY();
    float waYExpMinSumY = gPin->gNet()->waYExpMinSumY();

    gradientMinY
        = (waExpMinSumY * (gPin->minExpSumY() * (1.0 - wlCoeffY * gPin->cy()))
           + wlCoeffY * gPin->minExpSumY() * waYExpMinSumY)
          / (waExpMinSumY * waExpMinSumY);
  }

  // max y
  if (gPin->hasMaxExpSumY()) {
    float waExpMaxSumY = gPin->gNet()->waExpMaxSumY();
    float waYExpMaxSumY = gPin->gNet()->waYExpMaxSumY();

    gradientMaxY
        = (waExpMaxSumY * (gPin->maxExpSumY() * (1.0 + wlCoeffY * gPin->cy()))
           - wlCoeffY * gPin->maxExpSumY() * waYExpMaxSumY)
          / (waExpMaxSumY * waExpMaxSumY);
  }

  debugPrint(log_,
             GPL,
             "replace",
             5,
             "getGradientWAPin:  gradient:  X[{:g} {:g}]  Y[{:g} {:g}]",
             gradientMinX,
             gradientMaxX,
             gradientMinY,
             gradientMaxY);

  return FloatPoint(gradientMinX - gradientMaxX, gradientMinY - gradientMaxY);
}

FloatPoint NesterovBase::getWireLengthPreconditioner(const GCell* gCell) const
{
  return FloatPoint(gCell->gPins().size(), gCell->gPins().size());
}

FloatPoint NesterovBase::getDensityPreconditioner(const GCell* gCell) const
{
  float areaVal
      = static_cast<float>(gCell->dx()) * static_cast<float>(gCell->dy());

  return FloatPoint(areaVal, areaVal);
}

FloatPoint NesterovBase::getSumPreconditioner(const GCell* gCell,
                                              float densityPenalty,
                                              float minPreconditioner) const
{
  FloatPoint wireLengthPreCondi = getWireLengthPreconditioner(gCell);
  FloatPoint densityPrecondi = getDensityPreconditioner(gCell);

  FloatPoint sumPrecondi(
      wireLengthPreCondi.x + densityPenalty * densityPrecondi.x,
      wireLengthPreCondi.y + densityPenalty * densityPrecondi.y);

  if (sumPrecondi.x <= minPreconditioner) {
    sumPrecondi.x = minPreconditioner;
  }

  if (sumPrecondi.y <= minPreconditioner) {
    sumPrecondi.y = minPreconditioner;
  }
  return sumPrecondi;
}

FloatPoint NesterovBase::getDensityGradient(const GCell* gCell) const
{
  if (nbVars_.cellDensityForceCalPolicy == "field_interpolation") {
    return getDensityGradientByFieldInterpolation(gCell);
  } else {
    return getDensityGradientByCoveredBins(gCell);
  }
}

// get GCells' electroForcePair
// i.e. get DensityGradient with given GCell
FloatPoint NesterovBase::getDensityGradientByCoveredBins(
    const GCell* gCell) const
{
  std::pair<int, int> pairX = bg_.getDensityMinMaxIdxX(gCell);
  std::pair<int, int> pairY = bg_.getDensityMinMaxIdxY(gCell);

  FloatPoint electroForce;

  for (int i = pairX.first; i < pairX.second; i++) {
    for (int j = pairY.first; j < pairY.second; j++) {
      Bin* bin = bg_.binAt(i, j);
      float overlapArea = getOverlapDensityArea(bin, gCell);

      electroForce.x += overlapArea * bin->electroForceX();
      electroForce.y += overlapArea * bin->electroForceY();
    }
  }
  return electroForce;
}

FloatPoint NesterovBase::getDensityGradientByFieldInterpolation(
    const GCell* gCell) const
{
  auto r = bg_.getDensityFieldInterpolationBox(gCell);
  std::pair<int, int> binsIdx[2][2] = {
      std::make_pair(r.lx(), r.ly()),
      std::make_pair(r.lx(), r.uy()),
      std::make_pair(r.ux(), r.ly()),
      std::make_pair(r.ux(), r.uy()),
  };

  float binsForceX[2][2];
  float binsForceY[2][2];
  std::pair<int, int> binsCoords[2][2];
  for (auto i : {0, 1}) {
    for (auto j : {0, 1}) {
      int xIdx = binsIdx[i][j].first;
      int yIdx = binsIdx[i][j].second;
      const Bin* bin = nullptr;
      if (xIdx >= 0 && xIdx < bg_.binCntX() && yIdx >= 0
          && yIdx < bg_.binCntY()) {
        bin = bg_.binAt(xIdx, yIdx);
      }
      binsForceX[i][j] = bin ? bin->electroForceX() : 0;
      binsForceY[i][j] = bin ? bin->electroForceY() : 0;
      binsCoords[i][j] = bg_.getBinCenterCoord(xIdx, yIdx);
    }
  }

  const float x = gCell->dCx();
  const float y = gCell->dCy();
  const float x0 = binsCoords[0][0].first;
  const float y0 = binsCoords[0][0].second;
  const float x1 = binsCoords[1][1].first;
  const float y1 = binsCoords[1][1].second;
#if 0  // debug
  if (x < x0 || x > x1 || y < y0 || y > y1) {
    std::cout << std::endl;
    std::cout << "x=" << x << ", y=" << y << ", x0=" << x0 << ", y0=" << y0
              << ", x1=" << x1 << ", y1=" << y1 << std::endl;
    auto inst = gCell->instance()->dbInst();
    std::cout << "gCell:" << inst->getName() << "," << inst->getObjName() << ","
              << inst->getConstName() << std::endl;
  }
#endif

  assert(x1 > x0);
  assert(y1 > y0);
  assert(x >= x0 && x <= x1);
  assert(y >= y0 && y <= y1);

  const float dx0 = (x - x0) / (x1 - x0);
  const float dx1 = (x1 - x) / (x1 - x0);
  const float dy0 = (y - y0) / (y1 - y0);
  const float dy1 = (y1 - y) / (y1 - y0);
  const float dx[2] = {dx0, dx1};
  const float dy[2] = {dy0, dy1};

  // interpolation: f = f00*dx1*dy1 + f01*dx1*dy0 + f10*dx0*dy1 + f11*dx0*dy1
  FloatPoint electroForce;
  for (auto i : {0, 1}) {
    for (auto j : {0, 1}) {
      auto w = dx[(i + 1) % 2] * dy[(j + 1) % 2];
      electroForce.x += w * binsForceX[i][j];
      electroForce.y += w * binsForceY[i][j];
    }
  }

  // According to the OT theory, the field is the density movements
  // If the size of the cell is w*h, then:
  //   - move 1 unit on the x-dimension will have 1*h density changes
  //   - move 1 unit on the y-dimension will introduce 1*w density changes
  // So, the force on force_x = field_x / h, and forc_y = field / w;
  electroForce.x /= gCell->dy();
  electroForce.y /= gCell->dx();

#if 0
  auto cmp = getDensityGradientByCoveredBins(gCell);
  std::cout << std::endl;
  std::cout << "binsIndexes: ";
  for (auto i : {0, 1}) {
    for (auto j : {0, 1}) {
      int xIdx = binsIdx[i][j].first;
      int yIdx = binsIdx[i][j].second;
      std::cout << "(" << i << ", " << j << ")=" << "(" << xIdx << ", " << yIdx << "); ";
    }
  }
  std::cout << std::endl;
  std::cout << "x=" << x << ", y=" << y << ", x0=" << x0 << ", y0=" << y0
            << ", x1=" << x1 << ", y1=" << y1 << std::endl;
  std::cout << "x.cmp=" << cmp.x << ", x.log=" << electroForce.x << std::endl;
  std::cout << "y.cmp=" << cmp.x << ", y.log=" << electroForce.y << std::endl;
#endif
  return electroForce;
}

// Density force cals
void NesterovBase::updateDensityForceBin()
{
  // copy density to utilize FFT
  for (auto& bin : bg_.bins()) {
    fft_->updateDensity(bin->x(), bin->y(), bin->density());
  }

  // do FFT
  fft_->doFFT();

  // update electroPhi and electroForce
  // update sumPhi_ for nesterov loop
  sumPhi_ = 0;
  for (auto& bin : bg_.bins()) {
    auto eForcePair = fft_->getElectroForce(bin->x(), bin->y());
    bin->setElectroForce(eForcePair.first, eForcePair.second);

    float electroPhi = fft_->getElectroPhi(bin->x(), bin->y());
    bin->setElectroPhi(electroPhi);

    sumPhi_ += electroPhi * static_cast<float>(bin->densityArea());
  }
}

void NesterovBase::updateDbGCells()
{
  for (auto& gCell : gCells()) {
    if (gCell->isInstance()) {
      odb::dbInst* inst = gCell->instance()->dbInst();
      inst->setPlacementStatus(odb::dbPlacementStatus::PLACED);

      Instance* replInst = gCell->instance();
      // pad awareness on X coordinates
      inst->setLocation(
          gCell->dCx() - replInst->dx() / 2 + pb_->siteSizeX() * pb_->padLeft(),
          gCell->dCy() - replInst->dy() / 2);
    }
  }
}

int64_t NesterovBase::getHpwl()
{
  int64_t hpwl = 0;
  for (auto& gNet : gNets_) {
    gNet->updateBox();
    hpwl += gNet->hpwl();
  }
  return hpwl;
}

// int64_t is recommended, but float is 2x fast
static float getOverlapDensityArea(const Bin* bin, const GCell* cell)
{
  auto area = getOverlapArea(bin->rect(), cell->dRect());
  return area * cell->densityScale();
}

static int64_t getOverlapArea(const Bin* bin, const Instance* inst)
{
  return getOverlapArea(bin->rect(), inst->rect());
}

//
// https://codingforspeed.com/using-faster-exponential-approximation/
static float fastExp(float a)
{
  a = 1.0f + a / 1024.0f;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  a *= a;
  return a;
}

}  // namespace gpl
