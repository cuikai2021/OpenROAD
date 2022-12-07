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

#include "placerBase.h"

#include <odb/db.h>
#include <odb/dbTransform.h>

#include <iostream>

#include "fassert.hpp"
#include "nesterovBase.h"
#include "util.h"
#include "utl/Logger.h"
#include "linden/geom/rect.hpp"
#include "linden/geom/point.hpp"

namespace gpl {

using namespace odb;
using namespace std;
using utl::GPL;

static std::pair<int, int> getMinMaxIdx(int ll,
                                        int uu,
                                        int coreLL,
                                        int siteSize,
                                        int minIdx,
                                        int maxIdx);

static utl::Logger* slog_;

static bool isCoreAreaOverlap(Die& die, Instance& inst);

static int64_t getOverlapWithCoreArea(Die& die, Instance& inst);

////////////////////////////////////////////////////////
// Instance

Instance::Instance()
    : IntRect(),
      inst_(nullptr),
      extId_(INT_MIN),
      is_macro_(false),
      is_locked_(false),
      max_density_(1.0)
{
}

// for movable real instances
Instance::Instance(odb::dbInst* inst,
                   int padLeft,
                   int padRight,
                   int site_height,
                   utl::Logger* logger)
    : Instance()
{
  inst_ = inst;
  bool x_y_rotated = false;
  switch (inst->getOrient()) {
    case dbOrientType::R0:
    case dbOrientType::R180:
    case dbOrientType::MY:
    case dbOrientType::MX: {
      x_y_rotated = false;
    } break;
    case dbOrientType::R90:
    case dbOrientType::R270:
    case dbOrientType::MYR90:
    case dbOrientType::MXR90: {
      x_y_rotated = true;
    } break;
    default: {
      FASSERT(0 && "unknown orient ");
    } break;
  };

  dbBox* bbox = inst->getBBox();
  int lx_, ly_;
  inst->getLocation(lx_, ly_);
  auto master = inst_->getMaster();
  assert(master != nullptr);
  int64_t w = master->getWidth();
  int64_t h = master->getHeight();
  if (x_y_rotated) {
    std::swap(w, h);
  }
  auto ux_ = lx_ + w;
  auto uy_ = ly_ + h;

  if (isPlaceInstance()) {
    lx_ -= padLeft;
    ux_ += padRight;
  }
  rect_.reset(lx_, ly_, ux_, uy_);

  // Masters more than row_limit rows tall are treated as macros
  constexpr int row_limit = 6;

  if (inst->getMaster()->getType().isBlock()) {
    is_macro_ = true;
  } else if (bbox->getDY() > 6 * site_height) {
    is_macro_ = true;
    logger->warn(GPL,
                 134,
                 "Master {} is not marked as a BLOCK in LEF but is more "
                 "than {} rows tall.  It will be treated as a macro.",
                 inst->getMaster()->getName(),
                 row_limit);
  }

  // TODO
  // need additional adjustment
  // if instance (macro) is fixed and
  // its coordi is not multiple of rows' integer.
}

// for dummy instances
Instance::Instance(int lx, int ly, int ux, int uy)
    : IntRect(lx, ly, ux, uy), inst_(nullptr)
{
}

Instance::~Instance()
{
}

bool Instance::isFixed() const
{
  // dummy instance is always fixed
  if (isDummy()) {
    return true;
  }

  switch (inst_->getPlacementStatus()) {
    case dbPlacementStatus::NONE:
    case dbPlacementStatus::UNPLACED:
    case dbPlacementStatus::SUGGESTED:
    case dbPlacementStatus::PLACED:
      return false;
      break;
    case dbPlacementStatus::LOCKED:
    case dbPlacementStatus::FIRM:
    case dbPlacementStatus::COVER:
      return true;
      break;
  }
  return false;
}

bool Instance::isInstance() const
{
  return (inst_ != nullptr);
}

bool Instance::isPlaceInstance() const
{
  return (isInstance() && !isFixed());
}

bool Instance::isDummy() const
{
  return (inst_ == nullptr);
}

float Instance::max_density() const
{
  return max_density_;
}

float& Instance::max_density() {
  return max_density_;
}

void Instance::setLocation(int x, int y)
{
  rect_.moveTo(x, y);

  // pins update
  for (auto& pin : pins_) {
    pin->updateLocation(this);
  }
}

void Instance::setCenterLocation(int x, int y)
{
  rect_.moveDelta(x - cx(), y - cy());

  // pins update
  for (auto& pin : pins_) {
    pin->updateLocation(this);
  }
}

void Instance::dbSetPlaced()
{
  inst_->setPlacementStatus(dbPlacementStatus::PLACED);
}

void Instance::dbSetPlacementStatus(dbPlacementStatus ps)
{
  inst_->setPlacementStatus(ps);
}

void Instance::dbSetLocation()
{
  inst_->setLocation(lx(), ly());
}

void Instance::dbSetLocation(int x, int y)
{
  setLocation(x, y);
  dbSetLocation();
}

void Instance::dbSetCenterLocation(int x, int y)
{
  setCenterLocation(x, y);
  dbSetLocation();
}

int64_t Instance::area() const
{
  return rect_.area();
}

void Instance::addPin(Pin* pin)
{
  pins_.push_back(pin);
}

void Instance::setExtId(int extId)
{
  extId_ = extId;
}

bool Instance::isMacro() const
{
  return is_macro_;
}

bool Instance::isLocked() const
{
  return is_locked_;
}

void Instance::lock()
{
  is_locked_ = true;
}

void Instance::unlock()
{
  is_locked_ = false;
}

////////////////////////////////////////////////////////
// Pin

Pin::Pin()
    : term_(nullptr),
      inst_(nullptr),
      net_(nullptr),
      center_(),
      offsetCx_(0),
      offsetCy_(0),
      iTermField_(0),
      bTermField_(0),
      minPinXField_(0),
      minPinYField_(0),
      maxPinXField_(0),
      maxPinYField_(0)
{
}

Pin::Pin(odb::dbITerm* iTerm) : Pin()
{
  setITerm();
  term_ = (void*) iTerm;
  updateCoordi(iTerm);
}

Pin::Pin(odb::dbBTerm* bTerm) : Pin()
{
  setBTerm();
  term_ = (void*) bTerm;
  updateCoordi(bTerm);
}

std::string Pin::name() const
{
  if (!term_) {
    return "DUMMY";
  }
  if (isITerm()) {
    return dbITerm()->getInst()->getName() + '/'
           + dbITerm()->getMTerm()->getName();
  } else {
    return dbBTerm()->getName();
  }
}

void Pin::setITerm()
{
  iTermField_ = 1;
}

void Pin::setBTerm()
{
  bTermField_ = 1;
}

void Pin::setMinPin(DIMENSION dim)
{
  if (dim == X) {
    minPinXField_ = 1;
  } else {
    minPinYField_ = 1;
  }
}

void Pin::setMaxPin(DIMENSION dim)
{
  if (dim == X) {
    maxPinXField_ = 1;
  } else {
    maxPinYField_ = 1;
  }
}

void Pin::unsetMinPin(DIMENSION dim)
{
  if (dim == X) {
    minPinXField_ = 0;
  } else {
    minPinYField_ = 0;
  }
}

void Pin::unsetMaxPin(DIMENSION dim)
{
  if (dim == X) {
    maxPinXField_ = 0;
  } else {
    maxPinYField_ = 0;
  }
}

bool Pin::isITerm() const
{
  return (iTermField_ == 1);
}

bool Pin::isBTerm() const
{
  return (bTermField_ == 1);
}

bool Pin::isMinPin(DIMENSION dim) const
{
  return dim == X ? (minPinXField_ == 1) : (minPinYField_ == 1);
}

bool Pin::isMaxPin(DIMENSION dim) const
{
  return dim == X ? (maxPinXField_ == 1) : (maxPinYField_ == 1);
}

int Pin::cx() const
{
  return center_.getX();
}

int Pin::cy() const
{
  return center_.getY();
}

int Pin::offsetCx() const
{
  return offsetCx_;
}

int Pin::offsetCy() const
{
  return offsetCy_;
}

odb::dbITerm* Pin::dbITerm() const
{
  return (isITerm()) ? (odb::dbITerm*) term_ : nullptr;
}
odb::dbBTerm* Pin::dbBTerm() const
{
  return (isBTerm()) ? (odb::dbBTerm*) term_ : nullptr;
}

void Pin::updateCoordi(odb::dbITerm* iTerm)
{
  int offsetLx = INT_MAX;
  int offsetLy = INT_MAX;
  int offsetUx = INT_MIN;
  int offsetUy = INT_MIN;

  for (dbMPin* mPin : iTerm->getMTerm()->getMPins()) {
    for (dbBox* box : mPin->getGeometry()) {
      offsetLx = std::min(box->xMin(), offsetLx);
      offsetLy = std::min(box->yMin(), offsetLy);
      offsetUx = std::max(box->xMax(), offsetUx);
      offsetUy = std::max(box->yMax(), offsetUy);
    }
  }

  int lx = iTerm->getInst()->getBBox()->xMin();
  int ly = iTerm->getInst()->getBBox()->yMin();

  int instCenterX = iTerm->getInst()->getMaster()->getWidth() / 2;
  int instCenterY = iTerm->getInst()->getMaster()->getHeight() / 2;

  // Pin SHAPE is NOT FOUND;
  // (may happen on OpenDB bug case)
  if (offsetLx == INT_MAX || offsetLy == INT_MAX || offsetUx == INT_MIN
      || offsetUy == INT_MIN) {
    // offset is center of instances
    offsetCx_ = offsetCy_ = 0;
  }
  // usual case
  else {
    // offset is Pin BBoxs' center, so
    // subtract the Origin coordinates (e.g. instCenterX, instCenterY)
    //
    // Transform coordinates
    // from (origin: 0,0)
    // to (origin: instCenterX, instCenterY)
    //
    offsetCx_ = (offsetLx + offsetUx) / 2 - instCenterX;
    offsetCy_ = (offsetLy + offsetUy) / 2 - instCenterY;
  }

  center_.set(lx + instCenterX + offsetCx_, ly + instCenterY + offsetCy_);
}

//
// for BTerm, offset* will hold bbox info.
//
void Pin::updateCoordi(odb::dbBTerm* bTerm)
{
  int lx = INT_MAX;
  int ly = INT_MAX;
  int ux = INT_MIN;
  int uy = INT_MIN;

  for (dbBPin* bPin : bTerm->getBPins()) {
    Rect bbox = bPin->getBBox();
    lx = std::min(bbox.xMin(), lx);
    ly = std::min(bbox.yMin(), ly);
    ux = std::max(bbox.xMax(), ux);
    uy = std::max(bbox.yMax(), uy);
  }

  if (lx == INT_MAX || ly == INT_MAX || ux == INT_MIN || uy == INT_MIN) {
    string msg
        = string(bTerm->getConstName()) + " toplevel port is not placed!\n";
    msg += "       Replace will regard " + string(bTerm->getConstName())
           + " is placed in (0, 0)";
    slog_->warn(GPL, 1, msg);
  }

  // Just center
  offsetCx_ = offsetCy_ = 0;

  center_.set((lx + ux) / 2, (ly + uy) / 2);
}

void Pin::updateLocation(const Instance* inst)
{
  center_.set(inst->cx() + offsetCx_, inst->cy() + offsetCy_);
}

void Pin::setInstance(Instance* inst)
{
  inst_ = inst;
}

void Pin::setNet(Net* net)
{
  net_ = net;
}

bool Pin::isPlaceInstConnected() const
{
  if (!inst_) {
    return false;
  }
  return inst_->isPlaceInstance();
}

Pin::~Pin()
{
  term_ = nullptr;
  inst_ = nullptr;
  net_ = nullptr;
}

////////////////////////////////////////////////////////
// Net

Net::Net() : IntRect(), net_(nullptr)
{
}
Net::Net(odb::dbNet* net, bool skipIoMode) : Net()
{
  net_ = net;
  updateBox(skipIoMode);
}

Net::~Net()
{
}

int64_t Net::hpwl() const
{
  return rect_.dx() + rect_.dy();
}

void Net::updateBox(bool skipIoMode)
{
  bool first = true;
  for (dbITerm* iTerm : net_->getITerms()) {
    dbBox* box = iTerm->getInst()->getBBox();
    Rect r(box->xMin(), box->yMin(), box->xMax(), box->yMax());
    if (first) {
      rect_ = r;
      first = false;
    } else {
      rect_.merge(r);
    }
  }

  if (skipIoMode == false) {
    for (dbBTerm* bTerm : net_->getBTerms()) {
      for (dbBPin* bPin : bTerm->getBPins()) {
        const Rect& r = bPin->getBBox();
        if (first) {
          rect_ = r;
          first = false;
        } else {
          rect_.merge(r);
        }
      }
    }
  }
}

void Net::addPin(Pin* pin)
{
  pins_.push_back(pin);
}

odb::dbSigType Net::getSigType() const
{
  return net_->getSigType();
}

////////////////////////////////////////////////////////
// Die

Die::Die() : die_(), core_()
{
}

Die::Die(const odb::Rect& dieRect, const odb::Rect& coreRect) : Die()
{
  setDieBox(dieRect);
  setCoreBox(coreRect);
}

Die::~Die()
{
}

void Die::setDieBox(const odb::Rect& dieRect)
{
  die_ = dieRect;
}

void Die::setCoreBox(const odb::Rect& coreRect)
{
  core_ = coreRect;
}

PlacerBaseVars::PlacerBaseVars()
{
  reset();
}

void PlacerBaseVars::reset()
{
  padLeft = padRight = 0;
  skipIoMode = false;
}

////////////////////////////////////////////////////////
// PlacerBase

PlacerBase::PlacerBase()
    : db_(nullptr),
      log_(nullptr),
      pbVars_(),
      siteSizeX_(0),
      siteSizeY_(0),
      placeInstsArea_(0),
      nonPlaceInstsArea_(0),
      macroInstsArea_(0),
      stdInstsArea_(0)
{
}

PlacerBase::PlacerBase(odb::dbDatabase* db,
                       PlacerBaseVars pbVars,
                       utl::Logger* log)
    : PlacerBase()
{
  db_ = db;
  log_ = log;
  pbVars_ = pbVars;
  init();
}

PlacerBase::~PlacerBase()
{
  reset();
}

void PlacerBase::init()
{
  slog_ = log_;

  log_->info(GPL, 2, "DBU: {}", db_->getTech()->getDbUnitsPerMicron());

  dbBlock* block = db_->getChip()->getBlock();

  // die-core area update
  dbSet<dbRow> rows = block->getRows();
  odb::Rect coreRect;
  block->getCoreArea(coreRect);
  odb::Rect dieRect;
  block->getDieArea(dieRect);

  if (!dieRect.contains(coreRect)) {
    coreRect.print("coreRect:");
    dieRect.print("dieRect:");
    log_->error(GPL, 118, "core area outside of die.");
  }

  die_ = Die(dieRect, coreRect);

  // siteSize update
  dbRow* firstRow = *(rows.begin());
  siteSizeX_ = firstRow->getSite()->getWidth();
  siteSizeY_ = firstRow->getSite()->getHeight();

  log_->info(GPL, 3, "SiteSize: {} {}", siteSizeX_, siteSizeY_);
  log_->info(GPL, 4, "CoreAreaLxLy: {} {}", die_.coreLx(), die_.coreLy());
  log_->info(GPL, 5, "CoreAreaUxUy: {} {}", die_.coreUx(), die_.coreUy());

  // insts fill with real instances
  dbSet<dbInst> insts = block->getInsts();
  instStor_.reserve(insts.size());
  for (dbInst* inst : insts) {
    auto type = inst->getMaster()->getType();
    if (!type.isCore() && !type.isBlock() && type != dbMasterType::RING) {
      continue;
    }
    Instance myInst(inst,
                    pbVars_.padLeft * siteSizeX_,
                    pbVars_.padRight * siteSizeX_,
                    siteSizeY_,
                    log_);
    instStor_.push_back(myInst);

    dbBox* bbox = inst->getBBox();
    if (bbox->getDY() > die_.coreDy())
      log_->error(
          GPL, 119, "instance {} height is larger than core.", inst->getName());
    if (bbox->getDX() > die_.coreDx())
      log_->error(
          GPL, 120, "instance {} width is larger than core.", inst->getName());
  }

  // insts fill with fake instances (fragmented row or blockage)
  initInstsForUnusableSites();

  // init inst ptrs and areas
  insts_.reserve(instStor_.size());
  for (auto& inst : instStor_) {
    if (inst.isInstance()) {
      if (inst.isFixed()) {
        // Check whether fixed instance is
        // within the corearea
        //
        // outside of corearea is none of RePlAce's business
        if (isCoreAreaOverlap(die_, inst)) {
          fixedInsts_.push_back(&inst);
          nonPlaceInsts_.push_back(&inst);
          nonPlaceInstsArea_ += getOverlapWithCoreArea(die_, inst);
        }
      } else {
        placeInsts_.push_back(&inst);
        int64_t instArea = inst.area();
        placeInstsArea_ += instArea;
        // macro cells should be
        // macroInstsArea_
        if (inst.dy() > siteSizeY_ * 6) {
          macroInstsArea_ += instArea;
        }
        // smaller or equal height cells should be
        // stdInstArea_
        else {
          stdInstsArea_ += instArea;
        }
      }
      instMap_[inst.dbInst()] = &inst;
    } else if (inst.isDummy()) {
      dummyInsts_.push_back(&inst);
      nonPlaceInsts_.push_back(&inst);
      nonPlaceInstsArea_ += inst.area();
    }
    insts_.push_back(&inst);
  }

  // nets fill
  dbSet<dbNet> nets = block->getNets();
  netStor_.reserve(nets.size());

  size_t pin_cnt = 0;
  for (dbNet* net : nets) {
    dbSigType netType = net->getSigType();
    // escape nets with VDD/VSS/reset nets
    if (netType == dbSigType::SIGNAL || netType == dbSigType::CLOCK) {
      pin_cnt += net->getITerms().size();
      if (pbVars_.skipIoMode == false) {
        pin_cnt += net->getBTerms().size();
      }
    }
  }
  pinStor_.reserve(pin_cnt);
  for (dbNet* net : nets) {
    dbSigType netType = net->getSigType();

    // escape nets with VDD/VSS/reset nets
    if (netType == dbSigType::SIGNAL || netType == dbSigType::CLOCK) {
      Net myNet(net, pbVars_.skipIoMode);
      netStor_.push_back(myNet);

      // this is safe because of "reserve"
      Net* myNetPtr = &netStor_.back();
      netMap_[net] = myNetPtr;

      for (dbITerm* iTerm : net->getITerms()) {
        // note that, DB's ITerm can have
        // VDD/VSS pins.
        //
        // Escape those pins
        pinStor_.emplace_back(iTerm);
        Pin* pin = &pinStor_.back();
        pin->setNet(myNetPtr);
        auto inst = dbToPb(iTerm->getInst());
        pin->setInstance(inst);
        pinMap_[(void*) iTerm] = pin;

        // only CORE and BLOCK inst are in the instStor_
        // others, like PAD|RING|ENDCAP insts are not in the map
        if (inst) {
          inst->addPin(pin);
        }
        myNetPtr->addPin(pin);
      }

      if (pbVars_.skipIoMode == false) {
        for (dbBTerm* bTerm : net->getBTerms()) {
          pinStor_.emplace_back(bTerm);
          Pin* pin = &pinStor_.back();
          pin->setNet(myNetPtr);
          pinMap_[(void*) bTerm] = pin;
          myNetPtr->addPin(pin);
        }
      }
    }
  }
  assert(pinStor_.size() == pin_cnt);

  pins_.reserve(pinStor_.size());
  for (auto& pin : pinStor_) {
    pins_.push_back(&pin);
  }

  nets_.reserve(netStor_.size());
  for (auto& net : netStor_) {
    nets_.push_back(&net);
  }

  printInfo();
}

// Use dummy instance to fill unusable sites.  Sites are unusable
// due to fragmented rows or placement blockages.
void PlacerBase::initInstsForUnusableSites()
{
  dbSet<dbRow> rows = db_->getChip()->getBlock()->getRows();

  int siteCountX = (die_.coreUx() - die_.coreLx()) / siteSizeX_;
  int siteCountY = (die_.coreUy() - die_.coreLy()) / siteSizeY_;

  typedef enum {
      Empty,
      Row,
      FixedInst
    } SiteStatus;

  struct PlaceInfo
  {
    SiteStatus status;
    float max_density;
  };

  //
  // Initialize siteGrid as empty
  //
  std::vector<PlaceInfo> siteGrid(siteCountX * siteCountY, PlaceInfo{Empty, 1.0});

  // fill in rows' bbox
  for (dbRow* row : rows) {
    Rect rect;
    row->getBBox(rect);

    std::pair<int, int> pairX = getMinMaxIdx(
        rect.xMin(), rect.xMax(), die_.coreLx(), siteSizeX_, 0, siteCountX);

    std::pair<int, int> pairY = getMinMaxIdx(
        rect.yMin(), rect.yMax(), die_.coreLy(), siteSizeY_, 0, siteCountY);

    for (int i = pairX.first; i < pairX.second; i++) {
      for (int j = pairY.first; j < pairY.second; j++) {
        siteGrid[j * siteCountX + i] = {Row, 1.0};
      }
    }
  }

  // Mark blockage areas as empty so that their sites will be blocked.
  for (dbBlockage* blockage : db_->getChip()->getBlock()->getBlockages()) {
    dbInst* inst = blockage->getInstance();
    if (inst && !inst->isFixed()) {
      string msg
          = "Blockages associated with moveable instances "
            " are unsupported and ignored [inst: "
            + inst->getName() + "]\n";
      slog_->error(GPL, 3, msg);
      continue;
    }
    dbBox* bbox = blockage->getBBox();
    std::pair<int, int> pairX = getMinMaxIdx(
        bbox->xMin(), bbox->xMax(), die_.coreLx(), siteSizeX_, 0, siteCountX);

    std::pair<int, int> pairY = getMinMaxIdx(
        bbox->yMin(), bbox->yMax(), die_.coreLy(), siteSizeY_, 0, siteCountY);

    float blockage_density = blockage->getMaxDensity()/100.0; // convert maxdensity to a value between 0 and 1
    for (int j = pairY.first; j < pairY.second; j++) {
      for (int i = pairX.first; i < pairX.second; i++) {
        siteGrid[j * siteCountX + i] = {Empty, blockage_density};
      }
    }
  }

  // fill fixed instances' bbox
  for (auto& inst : instStor_) {
    if (!inst.isFixed()) {
      continue;
    }
    std::vector<linden::geom::Rect> obs_overlap_layer;
    auto master = inst.dbInst()->getMaster();
    for (auto obs : master->getObstructions()) {
      auto layer = obs->getTechLayer();
      if (layer->getType() == odb::dbTechLayerType::OVERLAP) {
    	  odb::Rect rect;
		  obs->getBox(rect);
		  odb::dbTransform transform;
		  inst.dbInst()->getTransform(transform);
		  transform.apply(rect);
        obs_overlap_layer.push_back(linden::geom::Rect(rect.xMin(), rect.yMin(), rect.xMax(), rect.yMax()));
      }
    }
    if (obs_overlap_layer.empty()) {
      linden::geom::Rect rect(
            inst.lx(), inst.ly(), inst.ux(), inst.uy());
      obs_overlap_layer.push_back(rect);
    }
    for (const auto& rect: obs_overlap_layer) {
      std::pair<int, int> pairX = getMinMaxIdx(
          rect.lx(), rect.ux(), die_.coreLx(), siteSizeX_, 0, siteCountX);
      std::pair<int, int> pairY = getMinMaxIdx(
          rect.ly(), rect.uy(), die_.coreLy(), siteSizeY_, 0, siteCountY);

      for (int i = pairX.first; i < pairX.second; i++) {
        for (int j = pairY.first; j < pairY.second; j++) {
          siteGrid[j * siteCountX + i] = {FixedInst, 1.0};
        }
      }
    }
  }

  //
  // Search the "Empty" coordinates on site-grid
  // --> These sites need to be dummyInstance
  //
  for (int j = 0; j < siteCountY; j++) {
    for (int i = 0; i < siteCountX; i++) {
      // if empty or partially empty spot found
      if (siteGrid[j * siteCountX + i].status == Empty) {
        int startX = i;
        float density = siteGrid[j * siteCountX + i].max_density;
        // find end points
        while (i < siteCountX && siteGrid[j * siteCountX + i].status == Empty
        && siteGrid[j * siteCountX + i].max_density == density) {
          i++;
        }
        int endX = i;
        Instance myInst(die_.coreLx() + siteSizeX_ * startX,
                        die_.coreLy() + siteSizeY_ * j,
                        die_.coreLx() + siteSizeX_ * endX,
                        die_.coreLy() + siteSizeY_ * (j + 1));
        myInst.max_density() = density;
        instStor_.push_back(myInst);
      }
    }
  }
}

void PlacerBase::reset()
{
  db_ = nullptr;
  pbVars_.reset();

  instStor_.clear();
  pinStor_.clear();
  netStor_.clear();

  pins_.clear();
  nets_.clear();
  insts_.clear();

  instMap_.clear();
  pinMap_.clear();
  netMap_.clear();

  placeInsts_.clear();
  fixedInsts_.clear();
  nonPlaceInsts_.clear();
}

int64_t PlacerBase::hpwl() const
{
  int64_t hpwl = 0;
  for (auto& net : nets_) {
    net->updateBox(pbVars_.skipIoMode);
    hpwl += net->hpwl();
  }
  return hpwl;
}

Instance* PlacerBase::dbToPb(odb::dbInst* inst) const
{
  auto instPtr = instMap_.find(inst);
  return (instPtr == instMap_.end()) ? nullptr : instPtr->second;
}

Pin* PlacerBase::dbToPb(odb::dbITerm* term) const
{
  auto pinPtr = pinMap_.find((void*) term);
  return (pinPtr == pinMap_.end()) ? nullptr : pinPtr->second;
}

Pin* PlacerBase::dbToPb(odb::dbBTerm* term) const
{
  auto pinPtr = pinMap_.find((void*) term);
  return (pinPtr == pinMap_.end()) ? nullptr : pinPtr->second;
}

Net* PlacerBase::dbToPb(odb::dbNet* net) const
{
  auto netPtr = netMap_.find(net);
  return (netPtr == netMap_.end()) ? nullptr : netPtr->second;
}

void PlacerBase::printInfo() const
{
  log_->info(GPL, 6, "NumInstances: {}", instStor_.size());
  log_->info(GPL, 7, "NumPlaceInstances: {}", placeInsts_.size());
  log_->info(GPL, 8, "NumFixedInstances: {}", fixedInsts_.size());
  log_->info(GPL, 9, "NumDummyInstances: {}", dummyInsts_.size());
  log_->info(GPL, 10, "NumNets: {}", nets_.size());
  log_->info(GPL, 11, "NumPins: {}", pins_.size());

  int maxFanout = INT_MIN;
  int sumFanout = 0;
  for (auto& net : nets_) {
    if (maxFanout < (int) net->pins().size()) {
      maxFanout = (int) net->pins().size();
    }
    sumFanout += (int) net->pins().size();
  }

  log_->info(GPL, 12, "DieAreaLxLy: {} {}", die_.dieLx(), die_.dieLy());
  log_->info(GPL, 13, "DieAreaUxUy: {} {}", die_.dieUx(), die_.dieUy());
  log_->info(GPL, 14, "CoreAreaLxLy: {} {}", die_.coreLx(), die_.coreLy());
  log_->info(GPL, 15, "CoreAreaUxUy: {} {}", die_.coreUx(), die_.coreUy());

  const int64_t coreArea = die_.coreArea();
  float util = static_cast<float>(placeInstsArea_)
               / (coreArea - nonPlaceInstsArea_) * 100;

  log_->info(GPL, 16, "CoreArea: {}", coreArea);
  log_->info(GPL, 17, "NonPlaceInstsArea: {}", nonPlaceInstsArea_);

  log_->info(GPL, 18, "PlaceInstsArea: {}", placeInstsArea_);
  log_->info(GPL, 19, "Util(%): {:.2f}", util);

  log_->info(GPL, 20, "StdInstsArea: {}", stdInstsArea_);
  log_->info(GPL, 21, "MacroInstsArea: {}", macroInstsArea_);

  if (util >= 100.1) {
    log_->error(GPL, 301, "Utilization exceeds 100%.");
  }
}

void PlacerBase::unlockAll()
{
  for (auto inst : insts_) {
    inst->unlock();
  }
}

static std::pair<int, int> getMinMaxIdx(int ll,
                                        int uu,
                                        int coreLL,
                                        int siteSize,
                                        int minIdx,
                                        int maxIdx)
{
  int lowerIdx = (ll - coreLL) / siteSize;
  int upperIdx = (util::fastModulo((uu - coreLL), siteSize) == 0)
                     ? (uu - coreLL) / siteSize
                     : (uu - coreLL) / siteSize + 1;
  return std::make_pair(std::max(minIdx, lowerIdx), std::min(maxIdx, upperIdx));
}

static bool isCoreAreaOverlap(Die& die, Instance& inst)
{
  int rectLx = max(die.coreLx(), inst.lx()),
      rectLy = max(die.coreLy(), inst.ly()),
      rectUx = min(die.coreUx(), inst.ux()),
      rectUy = min(die.coreUy(), inst.uy());
  return !(rectLx >= rectUx || rectLy >= rectUy);
}

static int64_t getRectOverlapWithCoreArea(Die& die, const IntRect& rect) {
    int rectLx = max(die.coreLx(), rect.lx()),
      rectLy = max(die.coreLy(), rect.ly()),
      rectUx = min(die.coreUx(), rect.ux()),
      rectUy = min(die.coreUy(), rect.uy());
  return static_cast<int64_t>(rectUx - rectLx)
         * static_cast<int64_t>(rectUy - rectLy);
}

static int64_t getOverlapWithCoreArea(Die& die, Instance& inst)
{
  std::vector<IntRect> obs_overlap_layer;
  if (inst.isMacro()) {
    auto master = inst.dbInst()->getMaster();
    for (auto const &obs : master->getObstructions()) {
      auto layer = obs->getTechLayer();
      if (layer->getType() == odb::dbTechLayerType::OVERLAP) {
    	  odb::Rect rect;
		  obs->getBox(rect);
		  odb::dbTransform transform;
		  inst.dbInst()->getTransform(transform);
		  transform.apply(rect);
        obs_overlap_layer.push_back(IntRect(rect.xMin(), rect.yMin(), rect.xMax(), rect.yMax()));
      }
    }
  }
  if (obs_overlap_layer.empty()) {
    return getRectOverlapWithCoreArea(die, inst);
  } else {
    int64_t res = 0;
    for (const auto& rect: obs_overlap_layer) {
      res += getRectOverlapWithCoreArea(die, rect);
    }
    return res;
  }


}

}  // namespace gpl
