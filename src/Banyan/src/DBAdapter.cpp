#include "DBAdapter.hpp"

#include <lemon/dijkstra.h>
#include <lemon/list_graph.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/polygon/polygon.hpp>
#include <cassert>
#include <deque>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "banyan_db.pb.h"
#include "odb/dbTypes.h"
#include "db_sta/dbNetwork.hh"
#include "db_sta/dbSta.hh"
#include "lef/lef.pb.h"
#include "odb/db.h"
#include "odb/dbShape.h"
#include "odb/dbWireCodec.h"
#include "odb/geom.h"
#include "ord/OpenRoad.hh"
#include "sta/PortDirection.hh"
#include "stt/SteinerTreeBuilder.h"
#include "utl/Logger.h"
#include "lrl/Laurel.hh"
#include "WireSorter.hpp"

namespace banyan {


namespace ply = boost::polygon;
using namespace boost::polygon::operators;

typedef ply::rectangle_data<int> PlyRect;
typedef ply::polygon_90_set_data<int> PlyPolygonSet;

inline namespace {
// work for both odb::Rect and odb::odbBox
template <typename T>
Rect* box2pb(T* ob)
{
  int xl = ob->xMin();
  int yl = ob->yMin();
  int xh = ob->xMax();
  int yh = ob->yMax();
  auto r = new Rect();
  r->mutable_lowleft()->set_x(xl);
  r->mutable_lowleft()->set_y(yl);
  r->mutable_upright()->set_x(xh);
  r->mutable_upright()->set_y(yh);
  return r;
}
}  // namespace

PinDirection OpenDBAdapter::odbIOType2Banyan(odb::dbIoType type)
{
  switch (type.getValue()) {
    case odb::dbIoType::INOUT:
      return PinDirection::INOUT;
    case odb::dbIoType::INPUT:
      return PinDirection::INPUT;
    case odb::dbIoType::OUTPUT:
      return PinDirection::OUTPUT;
    default:
      logger_->error(utl::BY, 1, "unsupport iotype {}", type.getString());
  }
}

PinDirection OpenDBAdapter::staPortDirection2Banyan(sta::PortDirection* pd)
{
  if (pd->isBidirect()) {
    return PinDirection::INOUT;
  }
  if (pd->isInput()) {
    return PinDirection::INPUT;
  }
  if (pd->isOutput() || pd->isTristate()) {
    return PinDirection::OUTPUT;
  }
  logger_->error(utl::BY, 2, "unsupport PortDirection {}", pd->name());
}

ComponentOrient OpenDBAdapter::odbOrient2Banyan(odb::dbOrientType type)
{
  switch (type) {
    case odb::dbOrientType::R0:
      return ComponentOrient::N;
    case odb::dbOrientType::R90:
      return ComponentOrient::W;
    case odb::dbOrientType::R180:
      return ComponentOrient::S;
    case odb::dbOrientType::R270:
      return ComponentOrient::E;
    case odb::dbOrientType::MY:
      return ComponentOrient::FN;
    case odb::dbOrientType::MX:
      return ComponentOrient::FS;
    case odb::dbOrientType::MYR90:
      return ComponentOrient::FE;
    case odb::dbOrientType::MXR90:
      return ComponentOrient::FW;
  }
  return ComponentOrient::N;
}

NetUse OpenDBAdapter::odbNetType2NetUse(odb::dbSigType sigType)
{
  switch (sigType.getValue()) {
    case odb::dbSigType::CLOCK:
      return NetUse::USE_CLOCK;
    case odb::dbSigType::SIGNAL:
      return NetUse::USE_SIGNAL;
    case odb::dbSigType::POWER:
      return NetUse::USE_POWER;
    case odb::dbSigType::GROUND:
      return NetUse::USE_GROUND;
    default:
      logger_->error(utl::BY, 1, "unknow net use {}", sigType.getString());
      return NetUse::NetUse_INT_MIN_SENTINEL_DO_NOT_USE_;
  }
}

std::optional<SpecialWireShape> OpenDBAdapter::odbSWireShape(
    odb::dbWireShapeType shapeType)
{
  switch (shapeType) {
    case odb::dbWireShapeType::RING:
      return SpecialWireShape::RING;
    case odb::dbWireShapeType::PADRING:
      return SpecialWireShape::PADRING;
    case odb::dbWireShapeType::BLOCKRING:
      return SpecialWireShape::BLOCKRING;
    case odb::dbWireShapeType::STRIPE:
      return SpecialWireShape::STRIPE;
    case odb::dbWireShapeType::FOLLOWPIN:
      return SpecialWireShape::FOLLOWPIN;
    case odb::dbWireShapeType::IOWIRE:
      return SpecialWireShape::IOWIRE;
    case odb::dbWireShapeType::COREWIRE:
      return SpecialWireShape::COREWIRE;
    case odb::dbWireShapeType::BLOCKWIRE:
      return SpecialWireShape::BLOCKWIRE;
    case odb::dbWireShapeType::BLOCKAGEWIRE:
      return SpecialWireShape::BLOCKAGEWIRE;
    case odb::dbWireShapeType::FILLWIRE:
      return SpecialWireShape::FILLWIRE;
    case odb::dbWireShapeType::DRCFILL:
      return SpecialWireShape::DRCFILL;
    case odb::dbWireShapeType::NONE:
      return std::nullopt;
    default:
      logger_->error(
          utl::BY, 1, "unknow special wire shape {}", shapeType.getString());
      return SpecialWireShape::SpecialWireShape_INT_MIN_SENTINEL_DO_NOT_USE_;
  }
}

std::string OpenDBAdapter::viaNameFromViaBox(odb::dbBox* box)
{
  assert(box->isVia());
  if (auto techVia = box->getTechVia(); techVia != nullptr) {
    return techVia->getName();
  }

  if (auto blockVia = box->getBlockVia(); blockVia != nullptr) {
    return blockVia->getName();
  }
  assert(false);

  return "";
}

sta::PortDirection* OpenDBAdapter::findStaPortDirection(std::string macroName,
                                                       std::string pinName)
{
  auto dbNetwork = ::ord::OpenRoad::openRoad()->getDbNetwork();
  if (dbNetwork == nullptr) {
    logger_->warn(utl::BY,
                  1,
                  "dbNetwork is empty, would use lef file to determine the "
                  "direction of macro' pins, instead of lib file");
    return nullptr;
  }
  auto staMaster = dbNetwork->findAnyCell(macroName.c_str());
  if (staMaster == nullptr) {
    logger_->warn(utl::BY,
                  1,
                  "No macro named {} was found in stalib, would use lef "
                  "file to determine the "
                  "direction of macro' pins, instead of lib file",
                  macroName);
    return nullptr;
  }
  auto libCell = dbNetwork->libertyCell(staMaster);
  if (libCell == nullptr) {
    logger_->warn(utl::BY,
                  1,
                  "No libCell named {} was found in stalib, would use lef "
                  "file to determine the "
                  "direction of macro' pins, instead of lib file",
                  macroName);
    return nullptr;
  }
  auto libPort = libCell->findLibertyPort(pinName.c_str());
  if (libPort == nullptr) {
    logger_->warn(utl::BY,
                  1,
                  "No pin named {} was found in stacell {}, would use "
                  "lef file to determine the "
                  "direction of macro' pins, instead of lib file",
                  pinName,
                  macroName);
    return nullptr;
  }
  return libPort->direction();
}

Pin OpenDBAdapter::odbITerm2Pin(odb::dbITerm* iterm)
{
  Pin pin;

  auto inst = iterm->getInst();
  auto mterm = iterm->getMTerm();

  auto instName = inst->getName();
  auto masterName = mterm->getMaster()->getName();
  auto pinName = mterm->getName();

  pin.set_instancename(instName);
  pin.mutable_portname()->set_value(pinName);

 
  if ( auto staDir  = findStaPortDirection(masterName, pinName); staDir != nullptr) {
    pin.set_direction(staPortDirection2Banyan(staDir));
  } else {
    pin.set_direction(odbIOType2Banyan(mterm->getIoType()));
  }

  return pin;
}

Pin OpenDBAdapter::odbBTerm2Pin(odb::dbBTerm* bterm)
{
  Pin pin;
  pin.set_instancename(bterm->getName());
  pin.set_direction(odbIOType2Banyan(bterm->getIoType()));
  return pin;
}

std::deque<Wire> OpenDBAdapter::odbWires2Wires(odb::dbWire* odbWires)
{
  std::deque<Wire> wires;

  auto doAddWire = [&](const std::string& layerName,
                       int x1,
                       int y1,
                       int ext1,
                       int x2,
                       int y2,
                       int ext2) {
    int beginX = x1;
    int beginY = y1;
    int beginExt = ext1;
    int endX = x2;
    int endY = y2;
    int endExt = ext2;

    assert(beginX == endX || beginY == endY);
    if (beginX > endX || beginY > endY) {
      std::swap(beginX, endX);
      std::swap(beginY, endY);
      std::swap(beginExt, endExt);
    }

    Wire wire;
    wire.set_layername(layerName);
    auto wireSeg = wire.add_segments();
    auto line = wireSeg->mutable_line();

    auto start = line->mutable_start();
    if (beginExt != DEFAULT_WIRE_EXT){
      start->mutable_extvalue()->set_value(beginExt);
    }else{
      start->clear_extvalue();
    }
    start->mutable_point()->set_x(beginX);
    start->mutable_point()->set_y(beginY);
    auto end = line->mutable_end();
    if (endExt != DEFAULT_WIRE_EXT) {
      end->mutable_extvalue()->set_value(endExt);
    }else{
      end->clear_extvalue();
    }
    end->mutable_point()->set_x(endX);
    end->mutable_point()->set_y(endY);

    wires.push_back(wire);
  };

  auto doAddRect = [&](const std::string& layerName,
                       int x,
                       int y,
                       int deltaX1,
                       int deltaY1,
                       int deltaX2,
                       int deltaY2) {
    int x1 = deltaX1;
    int y1 = deltaY1;
    int x2 = deltaX2;
    int y2 = deltaY2;

    Wire wire;
    wire.set_layername(layerName);
    auto wireSeg = wire.add_segments();
    auto wireRect = wireSeg->mutable_rect();

    auto point = wireRect->mutable_point();
    point->set_x(x);
    point->set_y(y);

    auto rect = wireRect->mutable_rect();
    rect->mutable_lowleft()->set_x(std::min(x1, x2));
    rect->mutable_lowleft()->set_y(std::min(y1, y2));
    rect->mutable_upright()->set_x(std::max(x1, x2));
    rect->mutable_upright()->set_y(std::max(y1, y2));

    wires.push_back(wire);
  };

  auto doAddVia = [&](const std::string& layerName,
                      const std::string& viaName,
                      int x,
                      int y) {
    Wire wire;
    wire.set_layername(layerName);
    auto wireSeg = wire.add_segments();
    auto wireVia = wireSeg->mutable_via();

    wireVia->set_vianame(viaName);

    auto point = wireVia->mutable_point();
    point->set_x(x);
    point->set_y(y);

    wires.push_back(wire);
  };

  odb::dbWireDecoder decode;
  int prevX = std::numeric_limits<int>::max();
  int prevY = std::numeric_limits<int>::max();
  int prevExt = std::numeric_limits<int>::max();
  bool hasPrevCoor = false;

  for (decode.begin(odbWires);;) {
    odb::dbWireDecoder::OpCode opcode = decode.next();

    switch (opcode) {
      case odb::dbWireDecoder::PATH:
      case odb::dbWireDecoder::SHORT:
      case odb::dbWireDecoder::VWIRE:
      case odb::dbWireDecoder::JUNCTION: {
        hasPrevCoor = false;
        break;
      }

      case odb::dbWireDecoder::POINT_VIRTUAL: {
        int x, y, ext;
        decode.getPoint(x, y);
        ext = DEFAULT_WIRE_EXT;
        prevX = x;
        prevY = y;
        prevExt = ext;
        hasPrevCoor = true;
        break;
      }

      case odb::dbWireDecoder::POINT: {
        int x, y, ext;
        decode.getPoint(x, y);
        ext = DEFAULT_WIRE_EXT;

        if (hasPrevCoor) {
          doAddWire(
              decode.getLayer()->getName(), prevX, prevY, prevExt, x, y, ext);
        }
        prevX = x;
        prevY = y;
        prevExt = ext;
        hasPrevCoor = true;
        break;
      }

      case odb::dbWireDecoder::POINT_EXT: {
        int x, y, ext;
        decode.getPoint(x, y, ext);
        if (hasPrevCoor) {
          doAddWire(
              decode.getLayer()->getName(), prevX, prevY, prevExt, x, y, ext);
        }
        prevX = x;
        prevY = y;
        prevExt = ext;
        hasPrevCoor = true;
        break;
      }

      case odb::dbWireDecoder::VIA: {
        assert(hasPrevCoor);
        doAddVia(decode.getLayer()->getName(),
                 decode.getVia()->getName(),
                 prevX,
                 prevY);
        break;
      }

      case odb::dbWireDecoder::TECH_VIA: {
        assert(hasPrevCoor);
        doAddVia(decode.getLayer()->getName(),
                 decode.getTechVia()->getName(),
                 prevX,
                 prevY);
        break;
      }

      case odb::dbWireDecoder::RECT: {
        assert(hasPrevCoor);

        int deltaX1;
        int deltaY1;
        int deltaX2;
        int deltaY2;
        decode.getRect(deltaX1, deltaY1, deltaX2, deltaY2);
        doAddRect(decode.getLayer()->getName(),
                  prevX,
                  prevY,
                  deltaX1,
                  deltaY1,
                  deltaX2,
                  deltaY2);
        break;
      }

      case odb::dbWireDecoder::ITERM:
      case odb::dbWireDecoder::BTERM:
      case odb::dbWireDecoder::RULE:
        break;

      case odb::dbWireDecoder::END_DECODE:
        return wires;

      default:
        break;
    }
  }
  return wires;
}

std::deque<SpecialWire> OpenDBAdapter::dbSWire2SpecialWires(
    odb::dbSWire* odbSWire)
{
  std::deque<SpecialWire> swires;
  for (auto odbSBox : odbSWire->getWires()) {
    SpecialWire swire;
    auto shapeType = odbSWireShape(odbSBox->getWireShapeType());
    if (shapeType.has_value()) {
      swire.set_shape(shapeType.value());
    }
    if (odbSBox->isVia()) {
      auto wireSeg = swire.add_segments();
      auto via = wireSeg->mutable_wirevia();
      int x;
      int y;
      odbSBox->getViaXY(x, y);

      via->set_vianame(viaNameFromViaBox(odbSBox));
      via->mutable_point()->set_x(x);
      via->mutable_point()->set_y(y);
    } else {
      swire.set_layername(odbSBox->getTechLayer()->getName());
      int x1 = odbSBox->xMin();
      int y1 = odbSBox->yMin();
      int x2 = odbSBox->xMax();
      int y2 = odbSBox->yMax();
      uint dx = x2 - x1;
      uint dy = y2 - y1;

      auto wireSeg = swire.add_segments();
      auto rect = wireSeg->mutable_rect();
      rect->mutable_lowleft()->set_x(x1);
      rect->mutable_lowleft()->set_y(y1);
      rect->mutable_upright()->set_x(x1 + dx);
      rect->mutable_upright()->set_y(y1 + dy);
    }
    swires.push_back(swire);
  }
  return swires;
}


void OpenDBAdapter::getDesignFromODB()
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();

  design_->set_name(block->getName());

  odb::Rect odbDieArea = block->getDieArea();
  auto dieArea = design_->mutable_diearea();
  auto dieAreaLL = dieArea->mutable_lowleft();
  dieAreaLL->set_x(odbDieArea.xMin());
  dieAreaLL->set_y(odbDieArea.yMin());
  auto dieAreaUR = dieArea->mutable_upright();
  dieAreaUR->set_x(odbDieArea.xMax());
  dieAreaUR->set_y(odbDieArea.yMax());

  odb::Rect odbCoreArea = block->getCoreArea();
  auto coreArea = design_->mutable_corearea();
  auto coreAreaLL = coreArea->mutable_lowleft();
  coreAreaLL->set_x(odbCoreArea.xMin());
  coreAreaLL->set_y(odbCoreArea.yMin());
  auto coreAreaUR = coreArea->mutable_upright();
  coreAreaUR->set_x(odbCoreArea.xMax());
  coreAreaUR->set_y(odbCoreArea.yMax());
}

template <typename odbVia>
Via* OpenDBAdapter::odbVia2Via(odbVia* odbvia)
{
  auto tech = this->db_->getTech();
  auto via = new Via();
  via->set_name(odbvia->getConstName());
  if constexpr (std::is_same_v<odbVia, odb::dbTechVia>)
    via->set_default_(odbvia->isDefault());
  else {
    auto techvia = odbvia->getTechVia();
    via->set_default_(techvia != nullptr && techvia->isDefault());
  }

  std::map<int, std::deque<Geometry>> idxToGeos;
  for (auto box : odbvia->getBoxes()) {
    auto layerIdx = box->getTechLayer()->getNumber();
    Geometry geo;
    geo.set_allocated_rect(box2pb(box));
    idxToGeos[layerIdx].push_back(geo);
  }
  int addedLayer = 0;
  for (auto const& [index, geos] : idxToGeos) {
    auto layerName = tech->findLayer(index)->getConstName();
    switch (addedLayer) {
      case 0: {
        auto botLayer = via->mutable_bottomlayer();
        botLayer->set_layername(layerName);
        for (auto const& x : geos) {
          *botLayer->mutable_geometries()->Add() = x;
        }
        break;
      }
      case 1: {
        auto cutLayer = via->mutable_cutlayer();
        cutLayer->set_layername(layerName);
        for (auto const& x : geos) {
          *cutLayer->mutable_geometries()->Add() = x;
        }
        break;
      }
      case 2: {
        auto topLayer = via->mutable_toplayer();
        topLayer->set_layername(layerName);
        for (auto const& x : geos) {
          *topLayer->mutable_geometries()->Add() = x;
        }
        break;
      }
    }
    addedLayer++;
  }
  return via;
}

void OpenDBAdapter::getDefViaFromODB()
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();

  for (auto via : block->getVias()) {
    auto techvia = via->getTechVia();
    if (!techvia || !techvia->getNonDefaultRule()) {
      design_->mutable_vias()->AddAllocated(odbVia2Via(via));
    }
  }
}

void OpenDBAdapter::getRoutingLayersFromODB()
{
  auto tech = db_->getTech();
  auto chip = db_->getChip();
  auto block = chip->getBlock();
  auto dieArea = design_->diearea();
  for (auto odbLayer : tech->getLayers()) {
    if (odbLayer->getType() != odb::dbTechLayerType::ROUTING) {
      continue;
    }
    auto odbTracks = block->findTrackGrid(odbLayer);
    if (odbTracks == nullptr) {
      logger_->error(
          utl::BY, 1, "routing layer {} empty tracks", odbLayer->getName());
    }

    auto track = design_->add_tracks();
    track->set_layername(odbLayer->getName());
    std::vector<int> grid;
    int tmin = 0, tmax = 0;
    if (odbLayer->getDirection() == odb::dbTechLayerDir::HORIZONTAL) {
      odbTracks->getGridY(grid);
      tmin = dieArea.lowleft().x();
      tmax = dieArea.upright().x();
    } else if (odbLayer->getDirection() == odb::dbTechLayerDir::VERTICAL) {
      odbTracks->getGridX(grid);
      tmin = dieArea.lowleft().y();
      tmax = dieArea.upright().y();
    } else {
      logger_->error(utl::BY,
                     1,
                     "unknow layer direction {} {}",
                     odbLayer->getName(),
                     odbLayer->getDirection().getString());
    }

    for (auto offset : grid) {
      auto line = track->add_lines();
      line->set_line(offset);
      line->set_min(tmin);
      line->set_max(tmax);
    }
  }
}

void OpenDBAdapter::getNDRsFromODB()
{
  auto chip = db_->getChip();
  auto tech = db_->getTech();
  auto block = chip->getBlock();

  std::vector<odb::dbTechLayerRule*> odbLayerRules;
  std::vector<odb::dbTechVia*> odbVias;
  std::vector<odb::dbTechViaGenerateRule*> odbViaRules;

  std::unordered_set<std::string> doneNDR;
  auto handleSingleNDR = [&](odb::dbTechNonDefaultRule* odbNDR) {
    std::string ndrName = odbNDR->getName();
    if (doneNDR.count(ndrName)) {
      logger_->warn(
          utl::BY, 1, "Found NDRs with the same name: \"{}\"", ndrName);
      return;
    }
    doneNDR.insert(ndrName);
    auto ndr = design_->add_nondefaultrules();

    ndr->set_name(odbNDR->getConstName());
    ndr->set_hardspacing(odbNDR->getHardSpacing());

    odbNDR->getLayerRules(odbLayerRules);
    for (auto odbLayerRule : odbLayerRules) {
      auto layerRule = ndr->add_layers();
      layerRule->set_layername(odbLayerRule->getLayer()->getConstName());
      layerRule->set_width(odbLayerRule->getWidth());
      if (auto sp = odbLayerRule->getSpacing(); sp != odbLayerRule->getLayer()->getSpacing()) {
        layerRule->mutable_spacing()->set_value(sp);
      }
      if (auto we = odbLayerRule->getWireExtension(); we != 0 && we != uint(odbLayerRule->getWidth() / 2)) {
        layerRule->mutable_wireextension()->set_value(we);
      }
    }

    odbNDR->getVias(odbVias);
    for (auto odbVia : odbVias) {
      ndr->mutable_vias()->AddAllocated(odbVia2Via(odbVia));
    }
    odbVias.clear();
    odbNDR->getUseVias(odbVias);
    for (auto odbVia : odbVias) {
      auto viaName = odbVia->getConstName();
      ndr->add_usevias()->assign(viaName);
    }
    odbVias.clear();

    odbNDR->getUseViaRules(odbViaRules);
    for (auto odbViaRule : odbViaRules) {
      auto viaRuleName = odbViaRule->getName();
      ndr->add_useviarules()->assign(viaRuleName);
    }
    odbViaRules.clear();

    for (auto odbLayer : tech->getLayers()) {
      int count;
      bool exist = odbNDR->getMinCuts(odbLayer, count);
      if (not exist) {
        continue;
      }
      auto min_cuts = ndr->add_mincuts();
      min_cuts->set_cutlayer(odbLayer->getConstName());
      min_cuts->set_cutsnum(count);
    }
  };
  // Banyan has its own lef parser, so only dump NDRs defined in DEF.
  for (auto odbNDR : block->getNonDefaultRules()) {
    handleSingleNDR(odbNDR);
  }
}

void OpenDBAdapter::getNetsFromODB()
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();

  for (auto odbNet : block->getNets()) {
    if (odbNet->isSpecial()) {
      continue;
    }
    auto net = design_->add_nets();
    net->set_name(odbNet->getName());
    net->set_use(odbNetType2NetUse(odbNet->getSigType()));

    for (auto iterm : odbNet->getITerms()) {
      *net->mutable_pins()->Add() = odbITerm2Pin(iterm);
    }
    for (auto bterm : odbNet->getBTerms()) {
      *net->mutable_pins()->Add() = odbBTerm2Pin(bterm);
    }
    auto odbNetWire = odbNet->getWire();
    if (odbNetWire != nullptr) {
      auto wires = odbWires2Wires(odbNet->getWire());
      for (auto const& x : wires) {
        *net->mutable_wires()->Add() = x;
      }
    }

    auto netNDR = odbNet->getNonDefaultRule();
    if (netNDR != nullptr) {
      auto ndr_name = netNDR->getName();
      net->mutable_nondefaultrule()->mutable_value()->assign(ndr_name);
    }
  }
}

void OpenDBAdapter::getSpecialNetsFromODB()
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();

  for (auto odbNet : block->getNets()) {
    if (!odbNet->isSpecial()) {
      continue;
    }
    auto snet = design_->add_specialnets();
    snet->set_name(odbNet->getName());
    snet->set_use(odbNetType2NetUse(odbNet->getSigType()));

    for (auto iterm : odbNet->getITerms()) {
      *snet->mutable_pins()->Add() = odbITerm2Pin(iterm);
    }
    for (auto bterm : odbNet->getBTerms()) {
      *snet->mutable_pins()->Add() = odbBTerm2Pin(bterm);
    }
    for (auto odbSwire : odbNet->getSWires()) {
      auto swires = dbSWire2SpecialWires(odbSwire);
      for (auto const& x : swires) {
        *snet->mutable_wires()->Add() = x;
      }
    }
  }
}

void OpenDBAdapter::getIOPinsFromODB()
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();
  for (auto bterm : block->getBTerms()) {
    auto pin = design_->add_pins();
    pin->set_name(bterm->getName());
    pin->set_direction(odbIOType2Banyan(bterm->getIoType()));

    for (auto bpin : bterm->getBPins()) {
      for (auto odbBox : bpin->getBoxes()) {
        auto box = pin->add_boxes();
        box->set_layername(odbBox->getTechLayer()->getName());
        auto rect = box->mutable_rect();
        rect->mutable_lowleft()->set_x(odbBox->xMin());
        rect->mutable_lowleft()->set_y(odbBox->yMin());
        rect->mutable_upright()->set_x(odbBox->xMax());
        rect->mutable_upright()->set_y(odbBox->yMax());
      }
    }
  }
}

void OpenDBAdapter::getInstancesFromODB()
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();
  for (auto instance : block->getInsts()) {
    auto master = instance->getMaster();
    auto component = design_->add_instances();
    component->set_name(instance->getName());
    component->set_macroname(master->getName());
    component->set_orient(odbOrient2Banyan(instance->getOrient()));

    auto oriPoint = component->mutable_origin();
    auto instOri = instance->getOrigin();
    oriPoint->set_x(instOri.x());
    oriPoint->set_y(instOri.y());

    int locationX = 0;
    int locationY = 0;
    instance->getLocation(locationX, locationY);
    auto locationPoint = component->mutable_location();
    locationPoint->set_x(locationX);
    locationPoint->set_y(locationY);
  }
}

void OpenDBAdapter::getBlockagesFromODB()
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();
  auto rtBlockageIds = block->getDieAreaRoutingBlockageIds();
  std::set<uint> rtBlockageIdSet(rtBlockageIds.begin(), rtBlockageIds.end());

  for (auto odbBlockage : block->getObstructions()) {
    auto layerName = odbBlockage->getBBox()->getTechLayer()->getName();

    auto obs = design_->add_blockages();
    BlockageUse use = BlockageUse::OBS;
    if (rtBlockageIdSet.find(odbBlockage->getId()) != rtBlockageIdSet.end()) {
      use = BlockageUse::DIEAREA_OBS;
    }
    obs->set_use(use);
    auto box = obs->add_boxes();
    box->set_layername(layerName);
    box->set_allocated_rect(box2pb(odbBlockage->getBBox()));
  }
}

void OpenDBAdapter::getRowsFromODB()
{
  for (auto row : db_->getChip()->getBlock()->getRows()) {
    if (row->getDirection() != odb::dbRowDir::HORIZONTAL)
      logger_->error(utl::BY, 9, "Banyan dont support non-horizontal row");
    odb::Rect b = row->getBBox();
    design_->mutable_rows()->AddAllocated(box2pb(&b));
  }
}

void OpenDBAdapter::getGcellsFromODB()
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();
  auto gcellGrid = block->getGCellGrid();
  if (gcellGrid == nullptr) {
    return;
  }
  auto designGcellGraph = design_->mutable_gcellgridgraph();
  std::vector<int> vLines;
  gcellGrid->getGridX(vLines);
  for (auto vLine : vLines) {
    designGcellGraph->add_vlines(vLine);
  }

  std::vector<int> hLines;
  gcellGrid->getGridY(hLines);
  for (auto hLine : hLines) {
    designGcellGraph->add_hlines(hLine);
  }

  auto numVPatterns = gcellGrid->getNumGridPatternsX();
  for (int i = 0; i < numVPatterns; i++) {
    int origin = 0;
    int lineCount = 0;
    int step = 0;
    gcellGrid->getGridPatternX(i, origin, lineCount, step);
    auto vgridparrten = designGcellGraph->add_vgridparrtens();
    vgridparrten->set_start(origin);
    vgridparrten->set_step(step);
    vgridparrten->set_nums(lineCount);
  }

  auto numHPatterns = gcellGrid->getNumGridPatternsY();
  for (int i = 0; i < numHPatterns; i++) {
    int origin = 0;
    int lineCount = 0;
    int step = 0;
    gcellGrid->getGridPatternY(i, origin, lineCount, step);
    auto hgridparrten = designGcellGraph->add_hgridparrtens();
    hgridparrten->set_start(origin);
    hgridparrten->set_step(step);
    hgridparrten->set_nums(lineCount);
  }
}

void OpenDBAdapter::loadDesignFromODB()
{
  design_.reset();
  design_ = std::make_unique<Design>();
  getDesignFromODB();
  getDefViaFromODB();
  getRoutingLayersFromODB();
  getIOPinsFromODB();
  getInstancesFromODB();
  getNDRsFromODB();
  getNetsFromODB();
  getSpecialNetsFromODB();
  getBlockagesFromODB();
  getRowsFromODB();
  getGcellsFromODB();
}

void OpenDBAdapter::loadDesignFromBanyanDB(std::string pbfile)
{
  design_.reset();
  BanyanCase bydb;
  std::ifstream dbfs;
  dbfs.open(pbfile);
  if (!bydb.ParseFromIstream(&dbfs)) {
    logger_->error(utl::BY, 10, "Parse {} BanyanDB fail", pbfile);
  }
  dbfs.close();
  design_ = std::unique_ptr<Design>(bydb.release_design());
}

void OpenDBAdapter::updateDB(Design& design)
{
  WireSorter::designWireOrdering(design);
  
  odb::dbBlock* block = db_->getChip()->getBlock();
  odb::dbTech* tech = db_->getTech();
  odb::dbWireEncoder _wire_encoder;

  auto pbVia2ODBVia = [&](const std::string& layerName, const ViaPoint& pbvia) {
    auto vianame = pbvia.vianame();
    odb::dbTechVia* tech_via = tech->findVia(vianame.c_str());
    odb::dbVia* db_via = tech_via ? nullptr : block->findVia(vianame.c_str());

    _wire_encoder.newPath(tech_via ? tech_via->getBottomLayer() : db_via->getBottomLayer(), odb::dbWireType("ROUTED"));

    auto point = pbvia.point();
    _wire_encoder.addPoint(point.x(), point.y());
    if (tech_via != nullptr) {
      _wire_encoder.addTechVia(tech_via);
    } else {
      _wire_encoder.addVia(db_via);
    }
  };

  auto pbWireline2ODBLine
      = [&](const std::string& layerName, const WireLine& line) {
          auto layerInOpenDB = tech->findLayer(layerName.c_str());
          _wire_encoder.newPath(layerInOpenDB, odb::dbWireType("ROUTED"));

          auto start = line.start().point();
          auto end = line.end().point();
          _wire_encoder.addPoint((int) start.x(), (int) start.y());
          _wire_encoder.addPoint((int) end.x(), (int) end.y());
        };

  auto pbWireRect2ODBRect
      = [&](const std::string& layerName, const WireRect& rect) {
          auto layerInOpenDB = tech->findLayer(layerName.c_str());
          _wire_encoder.newPath(layerInOpenDB, odb::dbWireType("ROUTED"));

          auto ll = rect.rect().lowleft();
          auto ur = rect.rect().upright();

          auto point = rect.point();
          _wire_encoder.addPoint((int) point.x(), (int) point.y());
          _wire_encoder.addRect(
              (int) ll.x(), (int) ll.y(), (int) ur.x(), (int) ur.y());
        };

  auto pbWire2ODBWire = [&](const Wire& pbWire) {
    auto layerName = pbWire.layername();
    for (auto pbWireSeg : pbWire.segments()) {
      if (pbWireSeg.has_line()) {
        pbWireline2ODBLine(layerName, pbWireSeg.line());
      } else if (pbWireSeg.has_rect()) {
        pbWireRect2ODBRect(layerName, pbWireSeg.rect());
      } else if (pbWireSeg.has_via()) {
        pbVia2ODBVia(layerName, pbWireSeg.via());
      }
    }
  };

  for (auto net : design.nets()) {
    auto netInOpenDB = block->findNet(net.name().c_str());
    if (netInOpenDB == nullptr) {
      continue;
    }
    laurel_->cacheNet(netInOpenDB);
    if (net.has_nondefaultrule()) {
      auto NDRName = net.nondefaultrule().value();
      auto odbNDR = tech->findNonDefaultRule(NDRName.c_str());
      netInOpenDB->setNonDefaultRule(odbNDR);
    } else {
      netInOpenDB->setNonDefaultRule(nullptr);
    }

    auto wire = netInOpenDB->getWire();
    if (wire != nullptr) {
      odb::dbWire::destroy(wire);
    }
    wire = odb::dbWire::create(netInOpenDB);
    _wire_encoder.begin(wire);

    for (auto pbWire : net.wires()) {
      pbWire2ODBWire(pbWire);
    }
    for (auto pbWire : net.pinaccesswires()) {
      pbWire2ODBWire(pbWire);
    }

    _wire_encoder.end();
    laurel_->diffNet(netInOpenDB);
  }
}

}  // namespace banyan
