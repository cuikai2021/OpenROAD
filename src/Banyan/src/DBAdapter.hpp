#pragma once

#include <deque>
#include <map>
#include <optional>
#include <utility>

#include "banyan_db.pb.h"
#include "odb/db.h"
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

namespace banyan {

constexpr int DEFAULT_WIRE_EXT = -1;

class OpenDBAdapter
{
 private:
  std::unique_ptr<Design> design_;

 protected:
  odb::dbDatabase* db_;
  utl::Logger* logger_;
  lrl::Laurel* laurel_;


  NetUse odbNetType2NetUse(odb::dbSigType sigType);
  std::optional<SpecialWireShape> odbSWireShape(odb::dbWireShapeType shapeType);

  // work for both odb::dbVia and odb::dbTechVia
  template<typename odbVia>
  Via* odbVia2Via(odbVia* odbvia);
  Pin odbITerm2Pin(odb::dbITerm* iterm);
  Pin odbBTerm2Pin(odb::dbBTerm* bterm);
  std::deque<SpecialWire> dbSWire2SpecialWires(odb::dbSWire* odbSWire);

  std::string viaNameFromViaBox(odb::dbBox* box);

  PinDirection odbIOType2Banyan(odb::dbIoType type);
  PinDirection staPortDirection2Banyan(sta::PortDirection* pd);
  ComponentOrient odbOrient2Banyan(odb::dbOrientType type);

  sta::PortDirection* findStaPortDirection(std::string macroName,
                                           std::string pinName);

  void getDesignFromODB();
  void getDefViaFromODB();
  void getRoutingLayersFromODB();
  void getNDRsFromODB();
  void getNetsFromODB();
  void getSpecialNetsFromODB();
  void getBlockagesFromODB();
  void getIOPinsFromODB();
  void getInstancesFromODB();
  void getRowsFromODB();
  void getGcellsFromODB();

 public:
  OpenDBAdapter(odb::dbDatabase* db,
                utl::Logger* logger,
                lrl::Laurel* laurel)
      : db_(db),
        logger_(logger),
        laurel_(laurel)
  {
    if (auto techlef = db->getTech(); techlef == nullptr) {
      logger_->error(
          utl::BY, 1, "techlef is empty, should read lef files first");
    }

    if (auto chip = db->getChip();
        chip == nullptr || chip->getBlock() == nullptr) {
      logger_->error(utl::BY, 1, "design is empty, should read def file first");
    }
  }

  ~OpenDBAdapter() {}

  void loadDesignFromODB();

  void loadDesignFromBanyanDB(std::string pbfile);

  void updateDB(Design& design);

  static std::deque<Wire> odbWires2Wires(odb::dbWire* odbWires);

  std::unique_ptr<Design> getDesign() { return std::move(design_); }
};

}  // namespace banyan
