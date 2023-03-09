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
#include <unordered_map>
#include <vector>

#include "dimension.h"
#include "rect.h"

namespace odb {
class dbDatabase;

class dbInst;
class dbITerm;
class dbBTerm;
class dbNet;

class dbPlacementStatus;
class dbSigType;

class dbBox;

class Rect;

}  // namespace odb

namespace utl {
class Logger;
}

namespace gpl {

class Pin;
class Net;

class Instance : public IntRect
{
 public:
  Instance();
  Instance(odb::dbInst* inst,
           int padLeft,
           int padRight,
           int site_height,
           utl::Logger* logger);
  Instance(int lx, int ly, int ux, int uy);  // dummy instance
  ~Instance();

  odb::dbInst* dbInst() const { return inst_; }

  // a cell that no need to be moved.
  bool isFixed() const;

  // a instance that need to be moved.
  bool isInstance() const;

  bool isPlaceInstance() const;

  bool isMacro() const;

  // A placeable instance may be fixed during part of incremental placement.
  // It remains in the set of placeable objects though so as to simplify
  // the unlocking process.
  bool isLocked() const;
  void lock();
  void unlock();

  // Dummy is virtual instance to fill in
  // unusable sites.  It will have inst_ as nullptr
  bool isDummy() const;
  float max_density() const;
  float& max_density();

  void setLocation(int x, int y);
  void setCenterLocation(int x, int y);

  void dbSetPlaced();
  void dbSetPlacementStatus(odb::dbPlacementStatus ps);
  void dbSetLocation();
  void dbSetLocation(int x, int y);
  void dbSetCenterLocation(int x, int y);

  int64_t area() const;

  void setExtId(int extId);
  int extId() const { return extId_; }

  void addPin(Pin* pin);
  const std::vector<Pin*>& pins() const { return pins_; }

 private:
  odb::dbInst* inst_;
  std::vector<Pin*> pins_;
  int extId_;
  bool is_macro_;
  bool is_locked_;
  float max_density_;
};

class Pin
{
 public:
  Pin();
  explicit Pin(odb::dbITerm* iTerm);
  explicit Pin(odb::dbBTerm* bTerm);
  ~Pin();

  odb::dbITerm* dbITerm() const;
  odb::dbBTerm* dbBTerm() const;

  bool isITerm() const;
  bool isBTerm() const;
  bool isMinPin(DIMENSION dim) const;
  bool isMaxPin(DIMENSION dim) const;

  void setITerm();
  void setBTerm();

  void setUnplaceIOPin(bool flag) { isUnplaceIOPin_ = flag; }
  bool isUnplacedIOPin() { return isUnplaceIOPin_; }

  void setMinPin(DIMENSION dim);
  void setMaxPin(DIMENSION dim);
  void unsetMinPin(DIMENSION dim);
  void unsetMaxPin(DIMENSION dim);

  int cx() const;
  int cy() const;
  int c(DIMENSION dim) const { return dim == X ? cx() : cy(); }

  int offsetCx() const;
  int offsetCy() const;
  int offsetC(DIMENSION dim) const
  {
    return dim == X ? offsetCx() : offsetCy();
  }

  void updateLocation(const Instance* inst);

  void setInstance(Instance* inst);
  void setNet(Net* net);

  bool isPlaceInstConnected() const;

  Instance* instance() const { return inst_; }
  Net* net() const { return net_; }
  std::string name() const;

 private:
  void* term_;
  Instance* inst_;
  Net* net_;

  // pin center coordinate is enough
  // Pins' placed location.
  odb::Point center_;

  // offset coordinates inside instance.
  // origin point is center point of instance.
  // (e.g. (DX/2,DY/2) )
  // This will increase efficiency for bloating
  int offsetCx_;
  int offsetCy_;

  unsigned char iTermField_ : 1;
  unsigned char bTermField_ : 1;
  unsigned char isUnplaceIOPin_ : 1;
  unsigned char minPinXField_ : 1;
  unsigned char minPinYField_ : 1;
  unsigned char maxPinXField_ : 1;
  unsigned char maxPinYField_ : 1;

  void updateCoordi(odb::dbITerm* iTerm);
  void updateCoordi(odb::dbBTerm* bTerm);
};

class Net : public IntRect
{
 public:
  Net();
  Net(odb::dbNet* net, bool skipIoMode);
  ~Net();

  // HPWL: half-parameter-wire-length
  int64_t hpwl() const;

  void updateBox(bool skipIoMode = false);

  const std::vector<Pin*>& pins() const { return pins_; }

  odb::dbNet* dbNet() const { return net_; }
  odb::dbSigType getSigType() const;

  void addPin(Pin* pin);

 private:
  odb::dbNet* net_;
  std::vector<Pin*> pins_;
};

class Die
{
 public:
  Die();
  Die(const odb::Rect& dieBox, const odb::Rect& coreRect);
  ~Die();

  void setDieBox(const odb::Rect& dieBox);
  void setCoreBox(const odb::Rect& coreBox);

  int dieLx() const { return die_.xMin(); }
  int dieLy() const { return die_.yMin(); }
  int dieUx() const { return die_.xMax(); }
  int dieUy() const { return die_.yMax(); }

  int coreLx() const { return core_.xMin(); }
  int coreLy() const { return core_.yMin(); }
  int coreUx() const { return core_.xMax(); }
  int coreUy() const { return core_.yMax(); }

  int dieCx() const { return (dieLx() + dieUx()) / 2; }
  int dieCy() const { return (dieLy() + dieUy()) / 2; }
  int dieDx() const { return die_.dx(); }
  int dieDy() const { return die_.dy(); }

  int coreCx() const { return (coreLx() + coreUx()) / 2; }
  int coreCy() const { return (coreLy() + coreUy()) / 2; }
  int coreDx() const { return core_.dx(); }
  int coreDy() const { return core_.dy(); }

  int64_t dieArea() const { return die_.area(); }
  int64_t coreArea() const { return core_.area(); }

 private:
  odb::Rect die_;
  odb::Rect core_;
};

class PlacerBaseVars
{
 public:
  int padLeft;
  int padRight;
  bool skipIoMode;

  PlacerBaseVars();
  void reset();
};

class PlacerBase
{
 public:
  PlacerBase();
  // temp padLeft/Right before OpenDB supporting...
  PlacerBase(odb::dbDatabase* db, PlacerBaseVars pbVars, utl::Logger* log);
  ~PlacerBase();

  const std::vector<Instance*>& insts() const { return insts_; }
  const std::vector<Pin*>& pins() const { return pins_; }
  const std::vector<Net*>& nets() const { return nets_; }

  //
  // placeInsts : a real instance that need to be placed
  // fixedInsts : a real instance that is fixed (e.g. macros, tapcells)
  // dummyInsts : a fake instance that is for unusable site handling
  //
  // nonPlaceInsts : fixedInsts + dummyInsts to enable fast-iterate on Bin-init
  //
  const std::vector<Instance*>& placeInsts() const { return placeInsts_; }
  const std::vector<Instance*>& fixedInsts() const { return fixedInsts_; }
  const std::vector<Instance*>& dummyInsts() const { return dummyInsts_; }
  const std::vector<Instance*>& nonPlaceInsts() const { return nonPlaceInsts_; }

  Die& die() { return die_; }

  // Pb : PlacerBase
  Instance* dbToPb(odb::dbInst* inst) const;
  Pin* dbToPb(odb::dbITerm* pin) const;
  Pin* dbToPb(odb::dbBTerm* pin) const;
  Net* dbToPb(odb::dbNet* net) const;

  int siteSizeX() const { return siteSizeX_; }
  int siteSizeY() const { return siteSizeY_; }

  int padLeft() const { return pbVars_.padLeft; }
  int padRight() const { return pbVars_.padRight; }

  int64_t hpwl() const;
  void printInfo() const;

  int64_t placeInstsArea() const { return placeInstsArea_; }
  int64_t nonPlaceInstsArea() const { return nonPlaceInstsArea_; }
  int64_t dummyInstsArea() const { return dummyInstsArea_; }
  int64_t fixedInstsArea() const { return fixedInstsArea_; }
  int64_t macroInstsArea() const { return macroInstsArea_; }
  int64_t stdInstsArea() const { return stdInstsArea_; }

  odb::dbDatabase* db() const { return db_; }

  void unlockAll();

 private:
  odb::dbDatabase* db_;
  utl::Logger* log_;

  PlacerBaseVars pbVars_;

  Die die_;

  std::vector<Instance> instStor_;
  std::vector<Pin> pinStor_;
  std::vector<Net> netStor_;

  std::vector<Instance*> insts_;
  std::vector<Pin*> pins_;
  std::vector<Net*> nets_;

  std::unordered_map<odb::dbInst*, Instance*> instMap_;
  std::unordered_map<void*, Pin*> pinMap_;
  std::unordered_map<odb::dbNet*, Net*> netMap_;

  std::vector<Instance*> placeInsts_;
  std::vector<Instance*> fixedInsts_;
  std::vector<Instance*> dummyInsts_;
  std::vector<Instance*> nonPlaceInsts_;

  int siteSizeX_;
  int siteSizeY_;

  int64_t placeInstsArea_;
  int64_t nonPlaceInstsArea_;
  int64_t dummyInstsArea_;
  int64_t fixedInstsArea_;

  // macroInstsArea_ + stdInstsArea_ = placeInstsArea_;
  // macroInstsArea_ should be separated
  // because of target_density tuning
  int64_t macroInstsArea_;
  int64_t stdInstsArea_;

  void init();
  void initInstsForUnusableSites();
  void fixIOPinsLocation();

  void reset();
};

}  // namespace gpl
