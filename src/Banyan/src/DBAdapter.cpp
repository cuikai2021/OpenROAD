
#include "DBAdapter.hpp"

#include <assert.h>
#include <lemon/dijkstra.h>
#include <lemon/list_graph.h>

#include <boost/algorithm/string.hpp>
#include <boost/polygon/polygon.hpp>
#include <cmath>
#include <unordered_map>
#include <utility>

#include "ByAdapter.hpp"
#include "GridlessMaze.hpp"
#include "GroupedRange.hpp"
#include "db_sta/dbNetwork.hh"
#include "lrl/Laurel.hh"
#include "odb/ZException.h"
#include "odb/db.h"
#include "odb/dbShape.h"
#include "odb/dbTransform.h"
#include "odb/dbWireCodec.h"
#include "odb/geom.h"
#include "rsz/SteinerTree.hh"
#include "sta/Network.hh"
#include "sta/PortDirection.hh"
#include "stt/SteinerTreeBuilder.h"
#include "utl/Logger.h"

namespace banyan {

using Y2Height = std::pair<int, int>;

namespace ply = boost::polygon;
using namespace boost::polygon::operators;

typedef ply::rectangle_data<int> PlyRect;
typedef ply::polygon_90_set_data<int> PlyPolygonSet;

bool OpenDBAdapter::isLegalized()
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();

  std::unordered_map<Y2Height, std::vector<PlyRect>, pair_hash> rows;
  for (auto row : block->getMultiHeightRows()) {
    odb::Rect rowBox;
    row->getBBox(rowBox);
    auto y2h = std::make_pair(rowBox.yMin(), rowBox.dy());
    rows[y2h].emplace_back(
        PlyRect(rowBox.xMin(), rowBox.yMin(), rowBox.xMax(), rowBox.yMax()));
  }
  std::unordered_map<Y2Height, std::vector<PlyRect>, pair_hash> insts;
  for (auto inst : block->getInsts()) {
    int x = 0;
    int y = 0;
    inst->getLocation(x, y);
    auto y2h = std::make_pair(y, inst->getMaster()->getHeight());
    if (rows.count(y2h) <= 0) {
      return false;
    }
    insts[y2h].emplace_back(PlyRect(x,
                                    y,
                                    x + inst->getMaster()->getWidth(),
                                    y + inst->getMaster()->getHeight()));
  }

  for (auto yh2Insts : insts) {
    // auto yh = yh2Insts.first;
    auto rects = yh2Insts.second;
    PlyPolygonSet plyset;
    plyset += rects;
    if (ply::area(plyset.self_intersect()) > 0) {
      return false;
    }
  }

  for (auto yh2Rows : rows) {
    auto yh = yh2Rows.first;
    auto instsRects = insts[yh];
    if (instsRects.size() <= 0) {
      continue;
    }
    auto rowsRects = yh2Rows.second;
    PlyPolygonSet rowsPlyset;
    rowsPlyset += rowsRects;

    PlyPolygonSet instsPlyset;
    instsPlyset += instsRects;

    auto instsArea = ply::area(instsPlyset);
    auto region = instsPlyset.interact(rowsPlyset);

    if (instsArea != ply::area(region)) {
      return false;
    }
  }

  return true;
}

PinDirection OpenDBAdapter::odbIOType2Banyan(odb::dbIoType type)
{
  switch (type) {
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

MacroClass OpenDBAdapter::odbMacroClass2Banyan(odb::dbMaster* master)
{
  if (master->isBlock()) {
    return MacroClass::MACRO_BLOCK;
  } else if (master->isCore()) {
    return MacroClass::MACRO_CORE;
  } else if (master->isCover()) {
    return MacroClass::MACRO_COVER;
  } else if (master->isEndCap()) {
    return MacroClass::MACRO_ENDCAP;
  } else if (master->isFiller()) {
    return MacroClass::MACRO_ENDCAP;
  } else if (master->isPad()) {
    return MacroClass::MACRO_PAD;
  }
  return MacroClass::MACRO_RING;
}

NetUse OpenDBAdapter::odbNetType2NetUse(odb::dbSigType sigType)
{
  switch (sigType) {
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

Pin OpenDBAdapter::odbITerm2Pin(odb::dbITerm* iterm)
{
  Pin pin;

  auto inst = iterm->getInst();
  auto mterm = iterm->getMTerm();

  instPinNames.insert(inst->getName() + "_" + mterm->getName());

  pin.set_instancename(inst->getName());
  pin.mutable_portname()->set_value(mterm->getName());
  return pin;
}

Pin OpenDBAdapter::odbBTerm2Pin(odb::dbBTerm* bterm)
{
  Pin pin;
  pin.set_instancename(bterm->getName());
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
    start->mutable_extvalue()->set_value(beginExt);
    start->mutable_point()->set_x(beginX);
    start->mutable_point()->set_y(beginY);

    auto end = line->mutable_end();
    end->mutable_extvalue()->set_value(endExt);
    end->mutable_point()->set_x(endX);
    end->mutable_point()->set_y(endY);

    wires.push_back(wire);
  };

  auto doAddRect = [&](const std::string& layerName,
                       int x,
                       int y,
                       int ext,
                       int deltaX1,
                       int deltaY1,
                       int deltaX2,
                       int deltaY2) {
    int x1 = x + deltaX1;
    int y1 = y + deltaY1;
    int x2 = x + deltaX2;
    int y2 = y + deltaY2;

    Wire wire;
    wire.set_layername(layerName);
    auto wireSeg = wire.add_segments();
    auto wireRect = wireSeg->mutable_rect();

    auto point = wireRect->mutable_point();
    point->mutable_extvalue()->set_value(ext);
    point->mutable_point()->set_x(0);
    point->mutable_point()->set_y(0);

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
                      int y,
                      int ext) {
    Wire wire;
    wire.set_layername(layerName);
    auto wireSeg = wire.add_segments();
    auto wireVia = wireSeg->mutable_via();

    wireVia->set_vianame(viaName);

    auto point = wireVia->mutable_point();
    point->mutable_extvalue()->set_value(ext);
    point->mutable_point()->set_x(x);
    point->mutable_point()->set_y(y);

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
                 prevY,
                 prevExt);
        break;
      }

      case odb::dbWireDecoder::TECH_VIA: {
        assert(hasPrevCoor);
        doAddVia(decode.getLayer()->getName(),
                 decode.getTechVia()->getName(),
                 prevX,
                 prevY,
                 prevExt);
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
                  prevExt,
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
      via->mutable_point()->mutable_point()->set_x(x);
      via->mutable_point()->mutable_point()->set_y(y);
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

  odb::Rect odbDieArea;
  block->getDieArea(odbDieArea);
  auto dieArea = design_->mutable_diearea();
  auto dieAreaLL = dieArea->mutable_lowleft();
  dieAreaLL->set_x(odbDieArea.xMin());
  dieAreaLL->set_y(odbDieArea.yMin());
  auto dieAreaUR = dieArea->mutable_upright();
  dieAreaUR->set_x(odbDieArea.xMax());
  dieAreaUR->set_y(odbDieArea.yMax());

  odb::Rect odbCoreArea;
  block->getCoreArea(odbCoreArea);
  auto coreArea = design_->mutable_corearea();
  auto coreAreaLL = coreArea->mutable_lowleft();
  coreAreaLL->set_x(odbCoreArea.xMin());
  coreAreaLL->set_y(odbCoreArea.yMin());
  auto coreAreaUR = coreArea->mutable_upright();
  coreAreaUR->set_x(odbCoreArea.xMax());
  coreAreaUR->set_y(odbCoreArea.yMax());
}

void OpenDBAdapter::getDefViaFromODB()
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();
  auto tech = db_->getTech();
  std::map<std::string, int> layerNameIndex;
  std::map<int, std::string> indexToLayerName;

  int index = 0;
  for (auto layer : tech->getLayers()) {
    layerNameIndex[layer->getName()] = index;
    indexToLayerName[index] = layer->getName();
    index++;
  }

  for (auto via : block->getVias()) {
    auto pbVia = design_->add_vias();
    pbVia->set_name(via->getName());
    std::map<int, std::deque<Geometry>> idxToGeos;
    for (auto box : via->getBoxes()) {
      auto layerName = box->getTechLayer()->getName();
      auto layerIdx = layerNameIndex.at(layerName);
      Geometry geo;
      auto rect = geo.mutable_rect();
      rect->mutable_lowleft()->set_x(box->xMin());
      rect->mutable_lowleft()->set_y(box->yMin());
      rect->mutable_upright()->set_x(box->xMax());
      rect->mutable_upright()->set_y(box->yMax());
      idxToGeos[layerIdx].push_back(geo);
    }

    int addedLayer = 0;
    for (auto const& [index, geos] : idxToGeos) {
      auto layerName = indexToLayerName[index];
      switch (addedLayer) {
        case 0: {
          auto botLayer = pbVia->mutable_bottomlayer();
          botLayer->set_name(layerName);
          for (auto const& x : geos) {
            *botLayer->mutable_geometries()->Add() = x;
          }
          break;
        }
        case 1: {
          auto cutLayer = pbVia->mutable_cutlayer();
          cutLayer->set_name(layerName);
          for (auto const& x : geos) {
            *cutLayer->mutable_geometries()->Add() = x;
          }
          break;
        }
        case 2: {
          auto topLayer = pbVia->mutable_toplayer();
          topLayer->set_name(layerName);
          for (auto const& x : geos) {
            *topLayer->mutable_geometries()->Add() = x;
          }
          break;
        }
      }
      addedLayer++;
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

VirtualPin OpenDBAdapter::gmVirtualPin2VirtualPin(const GmVirtualPin& gmvpin)
{
  auto tech = db_->getTech();
  VirtualPin vpin;
  for (auto pinname : gmvpin.names) {
    auto pin = vpin.add_pins();
    assert(pinname.first != "" && pinname.second != "");
    pin->set_instancename(pinname.first);
    pin->mutable_portname()->set_value(pinname.second);
  }

  odb::dbTechLayer* techlayer = nullptr;
  for (auto layer : tech->getLayers()) {
    if (layer->getType() == odb::dbTechLayerType::ROUTING) {
      techlayer = layer;
      break;
    }
  }

  auto wire = vpin.add_wires();
  wire->set_layername(techlayer->getName());

  for (auto line : gmvpin.lines) {
    auto b = line.toBox(techlayer->getWidth());
    auto wireSeg = wire->add_segments();
    auto wireRect = wireSeg->mutable_rect();

    auto point = wireRect->mutable_point();
    point->mutable_point()->set_x(b.xMin());
    point->mutable_point()->set_y(b.yMin());

    auto rect = wireRect->mutable_rect();
    rect->mutable_lowleft()->set_x(0);
    rect->mutable_lowleft()->set_y(0);
    rect->mutable_upright()->set_x(b.dx());
    rect->mutable_upright()->set_y(b.dy());
  }

  return vpin;
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
      auto layerRule = ndr->add_layerrules();
      layerRule->set_layername(odbLayerRule->getLayer()->getConstName());
      layerRule->set_width(odbLayerRule->getWidth());
      layerRule->set_spacing(odbLayerRule->getSpacing());
      layerRule->set_wireext(odbLayerRule->getWireExtension());
    }

    odbNDR->getVias(odbVias);
    for (auto odbPin : odbVias) {
      auto viaName = odbPin->getConstName();
      ndr->add_vianames()->assign(viaName);
    }
    odbVias.clear();
    odbNDR->getUseVias(odbVias);
    for (auto odbPin : odbVias) {
      auto viaName = odbPin->getConstName();
      ndr->add_vianames()->assign(viaName);
    }
    odbVias.clear();

    odbNDR->getUseViaRules(odbViaRules);
    for (auto odbViaRule : odbViaRules) {
      auto viaRuleName = odbViaRule->getName();
      ndr->add_viarulenames()->assign(viaRuleName);
    }
    odbViaRules.clear();

    for (auto odbLayer : tech->getLayers()) {
      int count;
      bool exist = odbNDR->getMinCuts(odbLayer, count);
      if (not exist) {
        continue;
      }
      auto min_cuts = ndr->add_mincuts();
      min_cuts->set_layername(odbLayer->getConstName());
      min_cuts->set_count(count);
    }
  };
  // Ref: If a nondefault rule name collides with an existing LEF or DEF
  // nondefault rule name that has different parameters, the application should
  // use the DEF definition when reading this DEF file, though it can change the
  // DEF nondefault rule name to make it unique. So, handle NDRs in block before
  // NDRs in tech.
  for (auto odbNDR : block->getNonDefaultRules()) {
    handleSingleNDR(odbNDR);
  }
  for (auto odbNDR : tech->getNonDefaultRules()) {
    handleSingleNDR(odbNDR);
  }
}

void OpenDBAdapter::getNetsFromODB()
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();
  auto designLegalized = isLegalized();
  std::unordered_map<std::string, std::deque<VirtualPin>> net2Vpins;
  if (designLegalized) {
    logger_->warn(utl::BY, 1, "legalized design, run gridless maze on m1");
    auto bydb = ByAdapter(db_, logger_).getByDb();
    auto gm = GridlessMaze(db_, bydb, logger_, GridlessMaze::HIGH);
    auto vpins = gm.run();
    for (auto vpin : vpins) {
      if (net2Vpins.find(vpin.netName) != net2Vpins.end()) {
        net2Vpins[vpin.netName].push_back(gmVirtualPin2VirtualPin(vpin));
      } else {
        std::deque<VirtualPin> netVpins{gmVirtualPin2VirtualPin(vpin)};
        net2Vpins[vpin.netName] = netVpins;
      }
    }
  } else {
    logger_->warn(utl::BY, 1, "unlegalized design, skip gridless maze on m1");
  }

  for (auto odbNet : block->getNets()) {
    if (odbNet->isSpecial()) {
      continue;
    }
    auto net = design_->add_nets();
    net->set_name(odbNet->getName());
    net->set_use(odbNetType2NetUse(odbNet->getSigType()));

    auto vpins = net->mutable_virtualpins();
    for (const auto& vpin : net2Vpins[odbNet->getName()]) {
      *vpins->Add() = vpin;
    }

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
    macroNames.insert(master->getName());
    component->set_orient(odbOrient2Banyan(instance->getOrient()));
    auto point = component->mutable_origin();

    int x;
    int y;
    instance->getOrigin(x, y);
    point->set_x(x);
    point->set_y(y);
  }
}

void OpenDBAdapter::getMacrosFromODB()
{
  auto sta_db_network_ = ord::OpenRoad::openRoad()->getSta()->getDbNetwork();
  bool had_warn_no_liberty = false;
  for (auto macroName : macroNames) {
    auto master = db_->findMaster(macroName.c_str());
    assert(master != nullptr);
    auto macro = design_->add_macros();
    macro->set_name(master->getName());
    macro->set_width(master->getWidth());
    macro->set_height(master->getHeight());

    int x = 0;
    int y = 0;
    master->getOrigin(x, y);
    auto point = macro->mutable_origin();
    point->set_x(x);
    point->set_y(y);
    macro->set_mclass(odbMacroClass2Banyan(master));
    for (auto mterm : master->getMTerms()) {
      auto pin = macro->add_pins();
      pin->set_name(mterm->getName());

      auto sta_port = sta_db_network_->dbToSta(mterm);
      auto sta_lib_port = sta_db_network_->libertyPort(sta_port);
      if (sta_lib_port != nullptr) {
        auto sta_direction = sta_lib_port->direction();
        pin->set_direction(staPortDirection2Banyan(sta_direction));
      } else {
        if (not had_warn_no_liberty) {
          logger_->warn(
              utl::BY,
              4,
              "there is macro that is not contained in the libs, such as {}",
              macroName);
          had_warn_no_liberty = true;
        }
        pin->set_direction(odbIOType2Banyan(mterm->getIoType()));
      }
      for (auto mpin : mterm->getMPins()) {
        for (auto odbBox : mpin->getGeometry()) {
          auto geo = pin->add_port();
          if (odbBox->isVia()) {
            auto via = geo->mutable_via();

            int x;
            int y;
            odbBox->getViaXY(x, y);

            via->set_vianame(viaNameFromViaBox(odbBox));
            via->mutable_point()->mutable_point()->set_x(x);
            via->mutable_point()->mutable_point()->set_y(y);
          } else {
            odb::Rect odbRect;
            odbBox->getBox(odbRect);

            if (odbRect.xMax() <= odbRect.xMin()
                || odbRect.yMax() <= odbRect.yMin()) {
              logger_->info(utl::BY,
                            1,
                            "skip invalid rect {} {} {} {}",
                            odbRect.xMin(),
                            odbRect.yMin(),
                            odbRect.xMax(),
                            odbRect.yMax());
              continue;
            }

            auto box = geo->mutable_box();
            box->set_layername(odbBox->getTechLayer()->getName());
            auto rect = box->mutable_rect();
            rect->mutable_lowleft()->set_x(odbRect.xMin());
            rect->mutable_lowleft()->set_y(odbRect.yMin());
            rect->mutable_upright()->set_x(odbRect.xMax());
            rect->mutable_upright()->set_y(odbRect.yMax());
          }
        }
      }
    }
    for (auto obs : master->getObstructions()) {
      auto geo = macro->add_obss();

      std::string layerName = obs->getTechLayer()->getName();

      if (obs->xMax() <= obs->xMin() || obs->yMax() <= obs->yMin()) {
        logger_->info(utl::BY,
                      1,
                      "skip invalid rect {} {} {} {}",
                      obs->xMin(),
                      obs->yMin(),
                      obs->xMax(),
                      obs->yMax());
        continue;
      }

      auto box = geo->mutable_box();
      box->set_layername(layerName);
      auto rect = box->mutable_rect();
      rect->mutable_lowleft()->set_x(obs->xMin());
      rect->mutable_lowleft()->set_y(obs->yMin());
      rect->mutable_upright()->set_x(obs->xMax());
      rect->mutable_upright()->set_y(obs->yMax());
    }
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

    int xl = odbBlockage->getBBox()->xMin();
    int yl = odbBlockage->getBBox()->yMin();
    int xh = odbBlockage->getBBox()->xMax();
    int yh = odbBlockage->getBBox()->yMax();
    auto obs = design_->add_blockages();
    BlockageUse use = BlockageUse::OBS;
    if (rtBlockageIdSet.find(odbBlockage->getId()) != rtBlockageIdSet.end()) {
      use = BlockageUse::DIEAREA_OBS;
    }
    obs->set_use(use);
    auto box = obs->add_boxes();
    box->set_layername(layerName);
    auto rect = box->mutable_rect();
    rect->mutable_lowleft()->set_x(xl);
    rect->mutable_lowleft()->set_y(yl);
    rect->mutable_upright()->set_x(xh);
    rect->mutable_upright()->set_y(yh);
  }
}

void OpenDBAdapter::getWireBudgetingFromLrl()
{
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
  getMacrosFromODB();
  getNDRsFromODB();
  getNetsFromODB();
  getSpecialNetsFromODB();
  getBlockagesFromODB();
  getWireBudgetingFromLrl();
}

void OpenDBAdapter::loadDesignFromBanyanDB(std::string pbfile)
{
  design_.reset();
  BanyanCase bydb;
  std::ifstream dbfs;
  dbfs.open(pbfile);
  bydb.ParseFromIstream(&dbfs);
  dbfs.close();
  design_ = std::unique_ptr<Design>(bydb.release_design());
}

void OpenDBAdapter::updateDB(const Design& design)
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  odb::dbTech* tech = db_->getTech();
  odb::dbWireEncoder _wire_encoder;

  auto pbVia2ODBVia = [&](const std::string& layerName, const ViaPoint& pbvia) {
    auto layerInOpenDB = tech->findLayer(layerName.c_str());
    _wire_encoder.newPath(layerInOpenDB, odb::dbWireType("ROUTED"));

    auto point = pbvia.point().point();
    _wire_encoder.addPoint(point.x(), point.y());
    auto vianame = pbvia.vianame();
    odb::dbTechVia* tech_via = tech->findVia(vianame.c_str());
    if (tech_via != nullptr) {
      _wire_encoder.addTechVia(tech_via);
    } else {
      odb::dbVia* db_via = block->findVia(vianame.c_str());
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

          auto point = rect.point().point();
          _wire_encoder.addPoint(0, 0);
          auto ll = rect.rect().lowleft();
          auto ur = rect.rect().upright();
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

  auto updateAccessPoint = [&](const PinAccessPoint& pbAccessPoint) {
    auto layerName = pbAccessPoint.layername();
    switch (pbAccessPoint.ap_case()) {
      case PinAccessPoint::ApCase::kOnTrack:
        // do nothing
        break;
      case PinAccessPoint::ApCase::kDropVia: {
        auto dropvia = pbAccessPoint.dropvia();
        pbWire2ODBWire(dropvia.via());
        break;
      }
      case PinAccessPoint::ApCase::kCoverTracks: {
        auto covertracks = pbAccessPoint.covertracks();
        pbWire2ODBWire(covertracks.line());
        for (auto block : covertracks.blocks()) {
          pbWire2ODBWire(block);
        }
        break;
      }
      case PinAccessPoint::ApCase::kMazePaths: {
        auto mazepaths = pbAccessPoint.mazepaths();
        if (mazepaths.has_via()) {
          pbWire2ODBWire(mazepaths.via());
        }
        if (mazepaths.has_line()) {
          pbWire2ODBWire(mazepaths.line());
        }
        for (auto pbWire : mazepaths.blocks()) {
          pbWire2ODBWire(pbWire);
        }
        break;
      }
      default:
        // do nothing
        break;
    }
  };

  for (auto net : design.nets()) {
    auto netInOpenDB = block->findNet(net.name().c_str());
    if (netInOpenDB == nullptr) {
      continue;
    }

    auto wire = netInOpenDB->getWire();
    if (wire != nullptr) {
      odb::dbWire::destroy(wire);
    }
    wire = odb::dbWire::create(netInOpenDB);
    _wire_encoder.begin(wire);

    for (auto pbPin : net.pins()) {
      if (pbPin.aps_size() <= 0) {
        continue;
      }
      for (auto pbAccessPoint : pbPin.aps()) {
        updateAccessPoint(pbAccessPoint);
      }
    }

    for (auto pbVPin : net.virtualpins()) {
      for (auto pbAccessPoint : pbVPin.aps()) {
        updateAccessPoint(pbAccessPoint);
      }
      for (auto pbWire : pbVPin.wires()) {
        pbWire2ODBWire(pbWire);
      }
    }

    for (auto pbWire : net.wires()) {
      pbWire2ODBWire(pbWire);
    }

    _wire_encoder.end();
  }
}

}  // namespace banyan
