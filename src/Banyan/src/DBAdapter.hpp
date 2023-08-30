#pragma once

#include <deque>
#include <iostream>
#include <list>
#include <map>

#include "Global.hpp"
#include "GridlessMaze.hpp"
#include "banyan_db.pb.h"
#include "db.h"
#include "string.h"
#include "utl/Logger.h"

namespace odb {
class dbDatabase;
class dbBlock;
class dbObject;
class dbTechLayer;
class dbNet;
class dbBox;
class dbSBox;
class dbWire;
class dbVia;
class dbTechVia;
class dbTechNonDefaultRule;
class Rect;
class dbSigType;
class dbWire;
class dbITerm;
class dbBTerm;
class dbIoType;
class dbOrientType;
class dbMasterType;
}  // namespace odb

namespace utl {
class Logger;
}

namespace lrl {
class Laurel;
}

namespace sta {
class Pin;
class PortDirection;
class dbNetwork;
}

namespace stt {
class SteinerTreeBuilder;
}

namespace rsz {
class Resizer;
}

namespace banyan {

constexpr int DEFAULT_WIRE_EXT = 0;

class OpenDBAdapter
{
 private:
  std::unique_ptr<Design> design_;

 protected:
  odb::dbDatabase* db_;
  stt::SteinerTreeBuilder* sttBuilder_;
  utl::Logger* logger_;
  lrl::Laurel* laurel_;
  rsz::Resizer* resizer_;
  bool timingDriven_ ;

  std::set<std::string> instPinNames;
  std::set<std::string> macroNames;


  NetUse odbNetType2NetUse(odb::dbSigType sigType);
  std::optional<SpecialWireShape> odbSWireShape(odb::dbWireShapeType shapeType);

  Pin odbITerm2Pin(odb::dbITerm* iterm);
  Pin odbBTerm2Pin(odb::dbBTerm* bterm);
  std::deque<SpecialWire> dbSWire2SpecialWires(odb::dbSWire* odbSWire);

  std::string viaNameFromViaBox(odb::dbBox* box);

  PinDirection odbIOType2Banyan(odb::dbIoType type);
  PinDirection staPortDirection2Banyan(sta::PortDirection* pd);
  ComponentOrient odbOrient2Banyan(odb::dbOrientType type);
  MacroClass odbMacroClass2Banyan(odb::dbMaster* master);

  VirtualPin gmVirtualPin2VirtualPin(const GmVirtualPin& gmvpin);

  sta::Pin* pin2StaPin(sta::dbNetwork* network, Pin* pin);

  void getDesignFromODB();
  void getDefViaFromODB();
  void getRoutingLayersFromODB();
  void getNDRsFromODB();
  void getNetsFromODB();
  void getSpecialNetsFromODB();
  void getBlockagesFromODB();
  void getIOPinsFromODB();
  void getInstancesFromODB();
  void getMacrosFromODB();
  void getWireBudgetingFromLrl();
  bool isLegalized();

 public:
  OpenDBAdapter(odb::dbDatabase* db,
                stt::SteinerTreeBuilder* sttBuilder_,
                utl::Logger* logger,
                lrl::Laurel* laurel,
                rsz::Resizer* resizer,
                bool timingDriven = false)
      : db_(db),
        sttBuilder_(sttBuilder_),
        logger_(logger),
        laurel_(laurel),
        resizer_(resizer),
        timingDriven_(timingDriven)
  {
    if (auto techlef = db->getTech(); techlef == nullptr) {
      logger_->error(
          utl::BY, 1, "techlef is empty, should read lef files first");
    }

    if (auto chip = db->getChip();
        chip == nullptr || chip->getBlock() == nullptr) {
      logger_->error(utl::BY, 1, "design is empty, should read def file first");
    }

    if (auto libs = db->getLibs(); libs.empty()) {
      logger_->warn(
          utl::BY,
          3,
          "libs is empty, would use lef file to determine the direction of macro' pins, instead of lib file");
    }
  }

  ~OpenDBAdapter() {}

  void loadDesignFromODB();

  void loadDesignFromBanyanDB(std::string pbfile);

  void updateDB(const Design& design);

  static std::deque<Wire> odbWires2Wires(odb::dbWire* odbWires);

  std::unique_ptr<Design> getDesign() { return std::move(design_); }
};

}  // namespace banyan
