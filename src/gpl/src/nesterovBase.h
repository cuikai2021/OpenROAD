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

#ifndef __NESTEROV_BASE__
#define __NESTEROV_BASE__

#include <memory>
#include <unordered_map>
#include <vector>

#include "dimension.h"
#include "point.h"
#include "waWirelengthModel.h"

namespace odb {
class dbInst;
class dbITerm;
class dbBTerm;
class dbNet;
}  // namespace odb

namespace utl {
class Logger;
}

namespace gpl {

class Instance;
class Die;
class PlacerBase;

class Instance;
class Pin;
class Net;

class GPin;
class FFT;

class GCell
{
  GCell();

 public:
  // instance cell
  GCell(Instance* inst);

  // filler cell
  GCell(int cx, int cy, int dx, int dy);
  ~GCell();

  Instance* instance() const;
  const std::vector<GPin*>& gPins() const { return gPins_; }

  void addGPin(GPin* gPin);

  void setInstance(Instance* inst);
  void setFiller();

  // normal coordinates
  int lx() const { return lx_; }
  int ly() const { return ly_; }
  int ux() const { return ux_; }
  int uy() const { return uy_; }
  int cx() const { return (lx_ + ux_) / 2; }
  int cy() const { return (ly_ + uy_) / 2; }
  int dx() const { return ux_ - lx_; }
  int dy() const { return uy_ - ly_; }

  // virtual density coordinates
  int dLx() const { return dLx_; }
  int dLy() const { return dLy_; }
  int dUx() const { return dUx_; }
  int dUy() const { return dUy_; }
  int dCx() const { return (dUx_ + dLx_) / 2; }
  int dCy() const { return (dUy_ + dLy_) / 2; }
  int dDx() const { return dUx_ - dLx_; }
  int dDy() const { return dUy_ - dLy_; }

  void setLocation(int lx, int ly);
  void setCenterLocation(int cx, int cy);
  void setSize(int dx, int dy);

  void setDensityLocation(int dLx, int dLy);
  void setDensityCenterLocation(int dCx, int dCy);
  void setDensitySize(int dDx, int dDy);

  void setDensityScale(float densityScale);
  void setGradientX(float gradX);
  void setGradientY(float gradY);

  float gradientX() const { return gradientX_; }
  float gradientY() const { return gradientY_; }
  float densityScale() const { return densityScale_; }

  bool isInstance() const;
  bool isFiller() const;
  bool isMacroInstance() const;
  bool isStdInstance() const;

 private:
  Instance* inst_;
  std::vector<GPin*> gPins_;
  int lx_;
  int ly_;
  int ux_;
  int uy_;

  int dLx_;
  int dLy_;
  int dUx_;
  int dUy_;

  float densityScale_;
  float gradientX_;
  float gradientY_;
};

class GNet
{
  GNet();

 public:
  GNet(Net* net);
  ~GNet();

  Net* net() const { return net_; }
  const std::vector<GPin*>& gPins() const { return gPins_; }

  int lx() const { return lx_; }
  int ly() const { return ly_; }
  int ux() const { return ux_; }
  int uy() const { return uy_; }

  void setTimingWeight(float timingWeight);
  void setCustomWeight(float customWeight);

  float totalWeight() const { return timingWeight_ * customWeight_; }
  float timingWeight() const { return timingWeight_; }
  float customWeight() const { return customWeight_; }

  void addGPin(GPin* gPin);
  void updateBox();
  int64_t hpwl() const;

  void setDontCare();
  bool isDontCare() const;

  // clear WA(Weighted Average) variables.
  void clearWaVars();

  void addWaExpMinSumX(float v) { wa_[X].addWaExpMinSum(v); }
  void addWaXExpMinSumX(float v) { wa_[X].addWaXExpMinSum(v); }
  void addWaExpMaxSumX(float v) { wa_[X].addWaExpMaxSum(v); }
  void addWaXExpMaxSumX(float v) { wa_[X].addWaXExpMaxSum(v); }
  void addWaExpMinSumY(float v) { wa_[Y].addWaExpMinSum(v); }
  void addWaYExpMinSumY(float v) { wa_[Y].addWaXExpMinSum(v); }
  void addWaExpMaxSumY(float v) { wa_[Y].addWaExpMaxSum(v); }
  void addWaYExpMaxSumY(float v) { wa_[Y].addWaXExpMaxSum(v); }

  float waExpMinSumX() const { return wa_[X].waExpMinSum(); }
  float waXExpMinSumX() const { return wa_[X].waXExpMinSum(); }
  float waExpMaxSumX() const { return wa_[X].waExpMaxSum(); }
  float waXExpMaxSumX() const { return wa_[X].waXExpMaxSum(); }
  float waExpMinSumY() const { return wa_[Y].waExpMinSum(); }
  float waYExpMinSumY() const { return wa_[Y].waXExpMinSum(); }
  float waExpMaxSumY() const { return wa_[Y].waExpMaxSum(); }
  float waYExpMaxSumY() const { return wa_[Y].waXExpMaxSum(); }

 private:
  Net* net_;
  std::vector<GPin*> gPins_;

  int lx_;
  int ly_;
  int ux_;
  int uy_;

  float timingWeight_;
  float customWeight_;

  WAWireLengthModel wa_[DIMENSION_CNT];

  unsigned char isDontCare_ : 1;
};

class GPin
{
  GPin();

 public:
  GPin(Pin* pin);
  ~GPin();

  Pin* pin() const;

  GCell* gCell() const { return gCell_; }
  GNet* gNet() const { return gNet_; }

  void setGCell(GCell* gCell);
  void setGNet(GNet* gNet);

  int cx() const { return cx_; }
  int cy() const { return cy_; }

  // clear WA(Weighted Average) variables.
  void clearWaVars();

  void setMaxExpSumX(float maxExpSumX);
  void setMaxExpSumY(float maxExpSumY);
  void setMinExpSumX(float minExpSumX);
  void setMinExpSumY(float minExpSumY);

  float maxExpSumX() const { return maxExpSumX_; }
  float maxExpSumY() const { return maxExpSumY_; }
  float minExpSumX() const { return minExpSumX_; }
  float minExpSumY() const { return minExpSumY_; }

  bool hasMaxExpSumX() const { return (hasMaxExpSumX_ == 1); }
  bool hasMaxExpSumY() const { return (hasMaxExpSumY_ == 1); }
  bool hasMinExpSumX() const { return (hasMinExpSumX_ == 1); }
  bool hasMinExpSumY() const { return (hasMinExpSumY_ == 1); }

  void setCenterLocation(int cx, int cy);
  void updateLocation(const GCell* gCell);
  void updateDensityLocation(const GCell* gCell);

 private:
  Pin* pin_;
  GCell* gCell_;
  GNet* gNet_;

  int offsetCx_;
  int offsetCy_;
  int cx_;
  int cy_;

  // weighted average WL vals stor for better indexing
  // Please check the equation (4) in the ePlace-MS paper.
  //
  // maxExpSum_: holds exp(x_i/gamma)
  // minExpSum_: holds exp(-x_i/gamma)
  // the x_i is equal to cx_ variable.
  //
  float maxExpSumX_;
  float maxExpSumY_;

  float minExpSumX_;
  float minExpSumY_;

  // flag variables
  //
  // check whether
  // this pin is considered in a WA models.
  unsigned char hasMaxExpSumX_ : 1;
  unsigned char hasMaxExpSumY_ : 1;

  unsigned char hasMinExpSumX_ : 1;
  unsigned char hasMinExpSumY_ : 1;
};

class Bin
{
  Bin();

 public:
  Bin(int x, int y, int lx, int ly, int ux, int uy, float targetDensity);

  ~Bin();

  int x() const { return x_; }
  int y() const { return y_; }

  int lx() const { return lx_; }
  int ly() const { return ly_; }
  int ux() const { return ux_; }
  int uy() const { return uy_; }

  int cx() const { return (ux_ + lx_) / 2; }
  int cy() const { return (uy_ + ly_) / 2; }
  int dx() const { return (ux_ - lx_); }
  int dy() const { return (uy_ - ly_); }

  float electroPhi() const;
  float electroForceX() const;
  float electroForceY() const;
  float targetDensity() const;
  float density() const;

  void setDensity(float density);
  void setTargetDensity(float density);
  void setElectroForce(float electroForceX, float electroForceY);
  void setElectroPhi(float phi);

  void setNonPlaceArea(int64_t area) { nonPlaceArea_ = area; }
  void setInstPlacedArea(int64_t area) { instPlacedArea_ = area; }
  void setFillerArea(int64_t area) { fillerArea_ = area; }

  void addNonPlaceArea(int64_t area) { nonPlaceArea_ += area; }
  void addInstPlacedArea(int64_t area) { instPlacedArea_ += area; }
  void addFillerArea(int64_t area) { fillerArea_ += area; }

  const int64_t binArea() const;
  const int64_t nonPlaceArea() const { return nonPlaceArea_; }
  const int64_t instPlacedArea() const { return instPlacedArea_; }
  const int64_t fillerArea() const { return fillerArea_; }

 private:
  // index
  int x_;
  int y_;

  // coordinate
  int lx_;
  int ly_;
  int ux_;
  int uy_;

  int64_t nonPlaceArea_;
  int64_t instPlacedArea_;
  int64_t fillerArea_;

  float density_;
  float targetDensity_;  // will enable bin-wise density screening
  float electroPhi_;
  float electroForceX_;
  float electroForceY_;
};

//
// The bin can be non-uniform because of
// "integer" coordinates
//
class BinGrid
{
 public:
  BinGrid();
  BinGrid(Die* die);
  ~BinGrid();

  void setPlacerBase(const std::shared_ptr<PlacerBase> pb);
  void setLogger(utl::Logger* log);
  void setCorePoints(const Die* die);
  void setBinCnt(int binCntX, int binCntY);
  void setBinCntX(int binCntX);
  void setBinCntY(int binCntY);
  void setTargetDensity(float density);
  void updateBinsGCellDensityArea(const std::vector<GCell*>& cells);

  void initBins();

  // lx, ly, ux, uy will hold coreArea
  int lx() const { return lx_; }
  int ly() const { return ly_; }
  int ux() const { return ux_; }
  int uy() const { return uy_; }

  int cx() const { return (ux_ + lx_) / 2; }
  int cy() const { return (uy_ + ly_) / 2; }
  int dx() const { return (ux_ - lx_); }
  int dy() const { return (uy_ - ly_); }

  int binCntX() const { return binCntX_; }
  int binCntY() const { return binCntY_; }
  int binSizeX() const { return binSizeX_; }
  int binSizeY() const { return binSizeY_; }

  int64_t overflowArea() const { return overflowArea_; }

  // return bins_ index with given gcell
  std::pair<int, int> getDensityMinMaxIdxX(const GCell* gcell) const;
  std::pair<int, int> getDensityMinMaxIdxY(const GCell* gcell) const;

  std::pair<int, int> getMinMaxIdxX(const Instance* inst) const;
  std::pair<int, int> getMinMaxIdxY(const Instance* inst) const;

  const std::vector<Bin*>& bins() const { return bins_; }
  Bin* binAt(int xIdx, int yIdx) const { return bins_[yIdx * binCntX_ + xIdx]; }

  void updateBinsNonPlaceArea();

 private:
  std::vector<Bin> binStor_;
  std::vector<Bin*> bins_;
  std::shared_ptr<PlacerBase> pb_;
  utl::Logger* log_;
  int lx_;
  int ly_;
  int ux_;
  int uy_;
  int binCntX_;
  int binCntY_;
  int binSizeX_;
  int binSizeY_;
  float targetDensity_;
  int64_t overflowArea_;
  unsigned char isSetBinCntX_ : 1;
  unsigned char isSetBinCntY_ : 1;
};

class NesterovBaseVars
{
 public:
  float targetDensity;
  int binCntX;
  int binCntY;
  float minWireLengthForceBar;
  // temp variables
  unsigned char isSetBinCntX : 1;
  unsigned char isSetBinCntY : 1;
  unsigned char useUniformTargetDensity : 1;

  NesterovBaseVars();
  void reset();
};

class NesterovBase
{
  NesterovBase();

 public:
  NesterovBase(NesterovBaseVars nbVars,
               std::shared_ptr<PlacerBase> pb,
               utl::Logger* log);
  ~NesterovBase();

  const std::vector<GCell*>& gCells() const { return gCells_; }
  const std::vector<GCell*>& gCellInsts() const { return gCellInsts_; }
  const std::vector<GCell*>& gCellFillers() const { return gCellFillers_; }

  const std::vector<GNet*>& gNets() const { return gNets_; }
  const std::vector<GPin*>& gPins() const { return gPins_; }

  //
  // placerBase To NesterovBase functions
  //
  GCell* pbToNb(Instance* inst) const;
  GPin* pbToNb(Pin* pin) const;
  GNet* pbToNb(Net* net) const;

  //
  // OpenDB To NesterovBase functions
  //
  GCell* dbToNb(odb::dbInst* inst) const;
  GPin* dbToNb(odb::dbITerm* pin) const;
  GPin* dbToNb(odb::dbBTerm* pin) const;
  GNet* dbToNb(odb::dbNet* net) const;

  // update gCells with lx, ly
  void updateGCellLocation(const std::vector<FloatPoint>& points);

  // update gCells with cx, cy
  void updateGCellCenterLocation(const std::vector<FloatPoint>& points);

  void updateGCellDensityCenterLocation(const std::vector<FloatPoint>& points);

  int binCntX() const;
  int binCntY() const;
  int binSizeX() const;
  int binSizeY() const;
  int64_t overflowArea() const;

  const std::vector<Bin*>& bins() const { return bg_.bins(); }

  // filler cells / area control
  // will be used in Routability-driven loop
  int fillerDx() const;
  int fillerDy() const;
  int fillerCnt() const;
  int64_t fillerCellArea() const;
  int64_t whiteSpaceArea() const;
  int64_t movableArea() const;
  int64_t totalFillerArea() const;

  // bloating cell will change the following areas.
  int64_t stdInstsArea() const;
  int64_t macroInstsArea() const;

  // update
  // fillerArea, whiteSpaceArea, movableArea
  // and totalFillerArea after changing gCell's size
  void updateAreas();

  // update density sizes with changed dx and dy
  void updateDensitySize();

  // should be separately defined.
  // This is mainly used for NesterovLoop
  int64_t nesterovInstsArea() const;

  // sum phi and target density
  // used in NesterovPlace
  float sumPhi() const;

  //
  // return uniform (lower bound) target density
  // LB of target density is required for massive runs.
  //
  float uniformTargetDensity() const;

  // initTargetDensity is set by users
  // targetDensity is equal to initTargetDensity and
  // would be changed dynamically in RD loop
  //
  float initTargetDensity() const;
  float targetDensity() const;

  void setTargetDensity(float targetDensity);

  // RD can shrink the number of fillerCells.
  void cutFillerCells(int64_t targetFillerArea);

  void updateDensityCoordiLayoutInside(GCell* gcell);

  float getDensityCoordiLayoutInsideX(const GCell* gCell, float cx) const;
  float getDensityCoordiLayoutInsideY(const GCell* gCell, float cy) const;

  // WL force update based on WeightedAverage model
  // wlCoeffX : WireLengthCoefficient for X.
  //            equal to 1 / gamma_x
  // wlCoeffY : WireLengthCoefficient for Y.
  //            equal to 1 / gamma_y
  //
  // Gamma is described in the ePlaceMS paper.
  //
  void updateWireLengthForceWA(float wlCoeffX, float wlCoeffY);

  FloatPoint getWireLengthGradientPinWA(const GPin* gPin,
                                        float wlCoeffX,
                                        float wlCoeffY) const;

  FloatPoint getWireLengthGradientWA(const GCell* gCell,
                                     float wlCoeffX,
                                     float wlCoeffY) const;

  // for preconditioner
  FloatPoint getWireLengthPreconditioner(const GCell* gCell) const;

  FloatPoint getDensityPreconditioner(const GCell* gCell) const;

  FloatPoint getDensityGradient(const GCell* gCell) const;

  int64_t getHpwl();

  // update electrostatic forces within Bin
  void updateDensityForceBin();

  void updateDbGCells();

 private:
  NesterovBaseVars nbVars_;
  std::shared_ptr<PlacerBase> pb_;
  utl::Logger* log_;

  BinGrid bg_;
  std::unique_ptr<FFT> fft_;

  int fillerDx_, fillerDy_;
  int64_t whiteSpaceArea_;
  int64_t movableArea_;
  int64_t totalFillerArea_;

  int64_t stdInstsArea_;
  int64_t macroInstsArea_;

  std::vector<GCell> gCellStor_;
  std::vector<GNet> gNetStor_;
  std::vector<GPin> gPinStor_;

  std::vector<GCell*> gCells_;
  std::vector<GCell*> gCellInsts_;
  std::vector<GCell*> gCellFillers_;

  std::vector<GNet*> gNets_;
  std::vector<GPin*> gPins_;

  std::unordered_map<Instance*, GCell*> gCellMap_;
  std::unordered_map<Pin*, GPin*> gPinMap_;
  std::unordered_map<Net*, GNet*> gNetMap_;

  float sumPhi_;
  float targetDensity_;
  float uniformTargetDensity_;

  void init();
  void initFillerGCells();
  void initBinGrid();

  void reset();
};

}  // namespace gpl

#endif
