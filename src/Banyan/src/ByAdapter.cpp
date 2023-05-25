#include "ByAdapter.hpp"

namespace banyan {

std::string GetPinName(odb::dbITerm* dbIterm)
{
  auto inst = dbIterm->getInst();
  auto instName = inst->getName();
  auto mTerm = dbIterm->getMTerm();
  auto mName = mTerm->getName();
  auto pinName = instName + "@" + mName;
  return pinName;
}

void ByAdapter::getWireFromDb(
    std::unordered_map<std::string, std::shared_ptr<ByNet>>& netMap,
    std::unordered_map<std::string, std::shared_ptr<ByLayer>>& layerMap,
    std::unordered_map<std::string, std::shared_ptr<ByVia>>& viaMap)
{
  std::string curNet;
  odb::dbBlock* block = db_->getChip()->getBlock();
  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::vector<odb::Rect>>>
      layerNetBoxes;
  auto addLine
      = [&](const std::string& layerName, int x1, int y1, int x2, int y2) {
          auto width
              = db_->getTech()->findLayer(layerName.c_str())->getWidth() / 2;
          if (x1 > x2) {
            std::swap(x1, x2);
          }
          if (y1 > y2) {
            std::swap(y1, y2);
          }
          layerNetBoxes[layerName][curNet].emplace_back(
              x1 - width, y1 - width, x2 + width, y2 + width);
        };

  auto addRect = [&](const std::string& layerName,
                     int x,
                     int y,
                     int deltaX1,
                     int deltaY1,
                     int deltaX2,
                     int deltaY2) {
    int x1 = x + deltaX1;
    int y1 = y + deltaY1;
    int x2 = x + deltaX2;
    int y2 = y + deltaY2;
    layerNetBoxes[layerName][curNet].emplace_back(x1, y1, x2, y2);
  };
  auto addVia = [&](const std::string& layerName,
                    const std::string& viaName,
                    int x,
                    int y) {
    assert(viaMap.count(viaName));
    assert(layerMap.count(layerName));
    netMap[curNet]->mutableVias().emplace_back(std::make_shared<ByViaInst>(
        viaMap[viaName], layerMap[layerName], curNet, BsPoint(x, y)));
  };
  for (auto dbNet : block->getNets()) {
    if (dbNet->isSpecial()) {
      continue;
    }
    auto odbWires = dbNet->getWire();
    if (odbWires != nullptr) {
      curNet = dbNet->getName();
      odb::dbWireDecoder decode;
      int prevX = std::numeric_limits<int>::max();
      int prevY = std::numeric_limits<int>::max();
      bool hasPrevCoor = false;
      bool bkFlag = false;
      for (decode.begin(odbWires); !bkFlag;) {
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
            int x, y;
            decode.getPoint(x, y);
            prevX = x;
            prevY = y;
            hasPrevCoor = true;
            break;
          }

          case odb::dbWireDecoder::POINT: {
            int x, y;
            decode.getPoint(x, y);
            if (hasPrevCoor) {
              addLine(decode.getLayer()->getName(), prevX, prevY, x, y);
            }
            prevX = x;
            prevY = y;
            hasPrevCoor = true;
            break;
          }

          case odb::dbWireDecoder::POINT_EXT: {
            int x, y, ext;
            decode.getPoint(x, y, ext);
            if (hasPrevCoor) {
              addLine(decode.getLayer()->getName(), prevX, prevY, x, y);
            }
            prevX = x;
            prevY = y;
            hasPrevCoor = true;
            break;
          }

          case odb::dbWireDecoder::VIA: {
            assert(hasPrevCoor);
            addVia(decode.getLayer()->getName(),
                   decode.getVia()->getName(),
                   prevX,
                   prevY);
            break;
          }

          case odb::dbWireDecoder::TECH_VIA: {
            assert(hasPrevCoor);
            addVia(decode.getLayer()->getName(),
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
            addRect(decode.getLayer()->getName(),
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

          case odb::dbWireDecoder::END_DECODE: {
            bkFlag = true;
            break;
          }

          default:
            break;
        }
      }
    }
  }
  for (auto& [layerName, netBoxes] : layerNetBoxes) {
    for (auto& [netName, boxes] : netBoxes) {
      auto info = std::make_shared<ByBoxInfo>(layerName,
                                              netName,
                                              std::nullopt,
                                              std::nullopt,
                                              std::nullopt,
                                              ByBoxInfo::WIRE);
      for (const auto& b : boxes) {
        netMap[netName]->mutableWires().emplace_back(std::make_shared<ByBox>(
            b.xMin(), b.yMin(), b.xMax(), b.yMax(), info));
      }
    }
  }
  logger_->info(
      utl::BY, 1, "--------------- GetWireFromDb end ------------------");
}

std::shared_ptr<ByDb> banyan::ByAdapter::getByDb()
{
  time_t stTime = time(nullptr);
  auto chip = db_->getChip();
  auto block = chip->getBlock();
  auto tech = db_->getTech();

  odb::Rect ca;
  block->getCoreArea(ca);
  ByBox coreArea(ca.xMin(), ca.yMin(), ca.xMax(), ca.yMax());
  logger_->info(utl::BY,
                1,
                "coreArea:{},{} {},{}",
                coreArea.xMin(),
                coreArea.yMin(),
                coreArea.xMax(),
                coreArea.yMax());

  std::vector<std::shared_ptr<ByLayer>> layers;
  std::vector<std::shared_ptr<ByNet>> nets;
  std::vector<std::shared_ptr<ByInst>> insts;
  std::vector<std::shared_ptr<ByVia>> vias;
  std::vector<std::shared_ptr<ByBox>> obstructions;

  std::unordered_map<std::string, std::shared_ptr<ByLayer>> layerMap;
  std::unordered_map<std::string, std::shared_ptr<ByVia>> viaMap;

  layers.reserve(tech->getLayerCount());
  for (auto layer : tech->getLayers()) {
    layers.emplace_back(std::make_shared<ByLayer>(
        layer->getName(), ByLayer::getIndexType(layer->getType())));
    layerMap[layer->getName()] = layers.back();
  }

  vias.reserve(tech->getViaCount());
  for (auto via : tech->getVias()) {
    auto curVia
        = std::make_shared<ByVia>(via->getName(),
                                  layerMap[via->getTopLayer()->getName()],
                                  layerMap[via->getBottomLayer()->getName()]);

    std::unordered_map<std::string, std::vector<odb::dbBox*>> layerBoxes;
    for (auto b : via->getBoxes()) {
      layerBoxes[b->getTechLayer()->getName()].emplace_back(b);
    }
    for (auto& [layerName, boxes] : layerBoxes) {
      auto info = std::make_shared<ByBoxInfo>(layerName,
                                              std::nullopt,
                                              std::nullopt,
                                              std::nullopt,
                                              via->getName(),
                                              ByBoxInfo::VIA);
      for (auto b : boxes) {
        curVia->mutableBoxes().emplace_back(std::make_shared<ByBox>(
            b->xMin(), b->yMin(), b->xMax(), b->yMax(), info));
      }
    }
    viaMap[via->getName()] = curVia;
    vias.emplace_back(curVia);
  }
  for (auto via : block->getVias()) {
    auto curVia
        = std::make_shared<ByVia>(via->getName(),
                                  layerMap[via->getTopLayer()->getName()],
                                  layerMap[via->getBottomLayer()->getName()]);

    std::unordered_map<std::string, std::vector<odb::dbBox*>> layerBoxes;
    for (auto b : via->getBoxes()) {
      layerBoxes[b->getTechLayer()->getName()].emplace_back(b);
    }
    for (auto& [layerName, boxes] : layerBoxes) {
      auto info = std::make_shared<ByBoxInfo>(layerName,
                                              std::nullopt,
                                              std::nullopt,
                                              std::nullopt,
                                              via->getName(),
                                              ByBoxInfo::VIA);
      for (auto b : boxes) {
        curVia->mutableBoxes().emplace_back(std::make_shared<ByBox>(
            b->xMin(), b->yMin(), b->xMax(), b->yMax(), info));
      }
    }
    viaMap[via->getName()] = curVia;
    vias.emplace_back(curVia);
  }

  std::unordered_map<std::string, std::string> pin2net;
  std::unordered_map<std::string, std::vector<std::string>> net2pins;
  std::unordered_map<std::string, std::shared_ptr<ByNet>> netMap;
  nets.reserve(block->getNets().size());
  for (auto net : block->getNets()) {
    auto netPtr = std::make_shared<ByNet>(net->getName(), net->isSpecial());
    nets.emplace_back(netPtr);
    netMap[net->getName()] = netPtr;
    if (net->isSpecial()) {
      continue;
    }
    auto netName = net->getName();
    for (auto pin : net->getITerms()) {
      auto pinName = GetPinName(pin);
      pin2net[pinName] = netName;
      net2pins[netName].push_back(pinName);
    }
  }
  getWireFromDb(netMap, layerMap, viaMap);
  for (auto pow : block->getNets()) {
    if (pow->isSpecial()) {
      auto curNet = netMap[pow->getName()];
      for (auto sw : pow->getSWires()) {
        for (auto odbSBox : sw->getWires()) {
          if (odbSBox->isVia()) {
            int x, y;
            std::vector<odb::dbShape> vBoxes;
            odbSBox->getViaXY(x, y);
            odbSBox->getViaBoxes(vBoxes);

            for (const auto& box : vBoxes) {
              auto info
                  = std::make_shared<ByBoxInfo>(box.getTechLayer()->getName(),
                                                pow->getName(),
                                                std::nullopt,
                                                std::nullopt,
                                                "",
                                                ByBoxInfo::VIA);
              curNet->mutableWires().emplace_back(std::make_shared<ByBox>(
                  box.xMin(), box.yMin(), box.xMax(), box.yMax(), info));
            }
          } else {
            auto info = std::make_shared<ByBoxInfo>(
                odbSBox->getTechLayer()->getName(),
                pow->getName(),
                std::nullopt,
                std::nullopt,
                std::nullopt,
                ByBoxInfo::SPECIAL_WIRE);
            curNet->mutableWires().emplace_back(
                std::make_shared<ByBox>(odbSBox->xMin(),
                                        odbSBox->yMin(),
                                        odbSBox->xMax(),
                                        odbSBox->yMax(),
                                        info));
          }
        }
      }
    }
  }

  obstructions.reserve(block->getObstructions().size());
  for (auto obs : block->getObstructions()) {
    auto box = obs->getBBox();
    auto info = std::make_shared<ByBoxInfo>(box->getTechLayer()->getName(),
                                            std::nullopt,
                                            std::nullopt,
                                            std::nullopt,
                                            std::nullopt,
                                            ByBoxInfo::OBS);
    obstructions.emplace_back(std::make_shared<ByBox>(
        box->xMin(), box->yMin(), box->xMax(), box->yMax(), info));
  }

  insts.reserve(block->getInsts().size());
  for (auto inst : block->getInsts()) {
    auto curInst = std::make_shared<ByInst>(inst->getName());
    insts.emplace_back(curInst);
    int x = 0, y = 0;
    inst->getOrigin(x, y);
    odb::dbTransform transformer(inst->getOrient(), odb::Point(x, y));
    auto master = inst->getMaster();
    // inst obs
    for (auto odbSBox : master->getObstructions()) {
      if (odbSBox->isVia()) {
        int vx;
        int vy;
        std::vector<odb::dbShape> vBoxes;
        odbSBox->getViaXY(vx, vy);
        odbSBox->getViaBoxes(vBoxes);
        std::unordered_map<std::string, std::vector<odb::dbShape>> layerBoxes;
        for (const auto& box : vBoxes) {
          layerBoxes[box.getTechLayer()->getName()].emplace_back(box);
        }
        for (auto& [layerName, boxes] : layerBoxes) {
          auto info = std::make_shared<ByBoxInfo>(layerName,
                                                  std::nullopt,
                                                  inst->getName(),
                                                  std::nullopt,
                                                  "",
                                                  ByBoxInfo::VIA);
          for (const auto& box : boxes) {
            odb::Rect rb(box.xMin(), box.yMin(), box.xMax(), box.yMax());
            transformer.apply(rb);
            curInst->mutableObstructions().emplace_back(std::make_shared<ByBox>(
                rb.xMin(), rb.yMin(), rb.xMax(), rb.yMax(), info));
          }
        }
      } else {
        auto info
            = std::make_shared<ByBoxInfo>(odbSBox->getTechLayer()->getName(),
                                          std::nullopt,
                                          inst->getName(),
                                          std::nullopt,
                                          std::nullopt,
                                          ByBoxInfo::INST_OBS);
        odb::Rect rb(
            odbSBox->xMin(), odbSBox->yMin(), odbSBox->xMax(), odbSBox->yMax());
        transformer.apply(rb);
        curInst->mutableObstructions().emplace_back(std::make_shared<ByBox>(
            rb.xMin(), rb.yMin(), rb.xMax(), rb.yMax(), info));
      }
    }
    // inst pin
    for (auto mTerm : master->getMTerms()) {
      auto pinName = inst->getName() + "@" + mTerm->getName();
      if (pin2net.count(pinName)) {
        auto netName = pin2net[pinName];
        for (auto mPin : mTerm->getMPins()) {
          std::unordered_map<std::string, std::vector<odb::dbBox*>> layerBoxes;
          for (auto box : mPin->getGeometry()) {
            layerBoxes[box->getTechLayer()->getName()].emplace_back(box);
          }
          auto curPin
              = std::make_shared<ByPin>(inst->getName(), mTerm->getName());
          for (auto& [layerName, boxes] : layerBoxes) {
            auto info = std::make_shared<ByBoxInfo>(layerName,
                                                    netName,
                                                    inst->getName(),
                                                    mTerm->getName(),
                                                    std::nullopt,
                                                    ByBoxInfo::INST_PIN);
            BsPolygon90Set s;
            for (auto box : boxes) {
              odb::Rect rb(box->xMin(), box->yMin(), box->xMax(), box->yMax());
              transformer.apply(rb);
              s.insert(bsp::rectangle_data<int>(
                  rb.xMin(), rb.yMin(), rb.xMax(), rb.yMax()));
            }
            std::vector<ByBox> maxRects;
            bsp::get_max_rectangles(maxRects, s);
            for (const auto& rb : maxRects) {
              curPin->mutableBoxes().emplace_back(std::make_shared<ByBox>(
                  rb.xMin(), rb.yMin(), rb.xMax(), rb.yMax(), info));
            }
          }
          curInst->mutableNetPins().emplace_back(curPin);
          netMap[netName]->mutablePins().emplace_back(curPin);
        }
      } else {
        for (auto mPin : mTerm->getMPins()) {
          std::unordered_map<std::string, std::vector<odb::dbBox*>> layerBoxes;
          for (auto box : mPin->getGeometry()) {
            layerBoxes[box->getTechLayer()->getName()].emplace_back(box);
          }
          auto curPin
              = std::make_shared<ByPin>(inst->getName(), mTerm->getName());
          for (auto& [layerName, boxes] : layerBoxes) {
            auto info = std::make_shared<ByBoxInfo>(layerName,
                                                    std::nullopt,
                                                    inst->getName(),
                                                    mTerm->getName(),
                                                    std::nullopt,
                                                    ByBoxInfo::INST_OBS);
            BsPolygon90Set s;
            for (auto box : boxes) {
              odb::Rect rb(box->xMin(), box->yMin(), box->xMax(), box->yMax());
              transformer.apply(rb);
              s.insert(bsp::rectangle_data<int>(
                  rb.xMin(), rb.yMin(), rb.xMax(), rb.yMax()));
            }
            std::vector<ByBox> maxRects;
            bsp::get_max_rectangles(maxRects, s);
            for (const auto& rb : maxRects) {
              curPin->mutableBoxes().emplace_back(std::make_shared<ByBox>(
                  rb.xMin(), rb.yMin(), rb.xMax(), rb.yMax(), info));
            }
          }
          curInst->mutableObsPin().emplace_back(curPin);
        }
      }
    }
  }

  std::vector<std::shared_ptr<ByBox>> rows;
  int msh = std::numeric_limits<int>::max();
  for (auto r : db_->getChip()->getBlock()->getRows()) {
    auto site = r->getSite();
    msh = std::min(msh, (int) site->getHeight());
  }
  for (auto r : db_->getChip()->getBlock()->getRows()) {
    auto site = r->getSite();
    if (site->getHeight() == msh) {
      odb::Rect b;
      r->getBBox(b);
      rows.emplace_back(
          std::make_shared<ByBox>(b.xMin(), b.yMin(), b.xMax(), b.yMax()));
    }
  }

  int wireCnt = 0;
  for (const auto& net : nets) {
    wireCnt += net->getWires().size();
  }
  int pinCnt = 0;
  for (const auto& inst : insts) {
    pinCnt += inst->getNetPins().size();
    pinCnt += inst->getObsPin().size();
  }

  auto dbptr = std::make_shared<ByDb>(
      coreArea, layers, nets, insts, vias, obstructions, rows);
  logger_->info(
      utl::BY,
      1,
      "layers:{} nets:{} insts:{} pins:{} vias:{} obs:{} wire:{}  time:{}s",
      layers.size(),
      nets.size(),
      insts.size(),
      pinCnt,
      vias.size(),
      obstructions.size(),
      wireCnt,
      time(nullptr) - stTime);
  return dbptr;
}

odb::dbTechLayer* ByAdapter::findFirstRoutingLayer(odb::dbTech* dbTech)
{
  for (auto layer : dbTech->getLayers()) {
    if (layer->getType() == odb::dbTechLayerType::ROUTING) {
      return layer;
    }
  }
  return nullptr;
}
void ByAdapter::updateWire(odb::dbDatabase* db,
                           const std::shared_ptr<ByDb>& bydb)
{
  odb::dbBlock* block = db->getChip()->getBlock();
  odb::dbTech* tech = db->getTech();
  odb::dbWireEncoder _wire_encoder;
  for (const auto& net : bydb->getNets()) {
    if (net->isSpecial()) {
      continue;
    }
    auto netInOpenDB = block->findNet(net->getName().c_str());
    if (netInOpenDB == nullptr) {
      continue;
    }
    auto wire = odb::dbWire::create(netInOpenDB);
    if (wire == nullptr) {
      wire = netInOpenDB->getWire();
    }
    odb::dbWire::destroy(wire);
  }
  for (const auto& net : bydb->getNets()) {
    if (net->isSpecial()) {
      continue;
    }
    auto netInOpenDB = block->findNet(net->getName().c_str());
    if (netInOpenDB == nullptr) {
      continue;
    }
    auto wire = odb::dbWire::create(netInOpenDB);
    if (wire == nullptr) {
      wire = netInOpenDB->getWire();
    }
    _wire_encoder.begin(wire);
    for (const auto& b : net->getWires()) {
      auto layerInOpenDB = tech->findLayer(b->getLayerName().c_str());
      auto lw = layerInOpenDB->getWidth();
      _wire_encoder.newPath(layerInOpenDB, odb::dbWireType("ROUTED"));
      if (b->getType() == ByBoxInfo::WIRE) {
        if (b->isLineBox(lw)) {
          b->narrow(lw / 2);
          assert((b->xMin() == b->xMax()) || (b->yMin() == b->yMax()));
          _wire_encoder.addPoint(b->xMin(), b->yMin());
          _wire_encoder.addPoint(b->xMax(), b->yMax());
        } else {
          _wire_encoder.addPoint(b->xMin(), b->yMin());
          _wire_encoder.addRect(
              0, 0, (b->xMax() - b->xMin()), (b->yMax() - b->yMin()));
        }
      }
    }
    for (auto via : net->getVias()) {
      auto layerInOpenDB = tech->findLayer(via->getLayer()->getName().c_str());
      _wire_encoder.newPath(layerInOpenDB, odb::dbWireType("ROUTED"));
      _wire_encoder.addPoint(via->x(), via->y());
      auto name = via->getVia()->getName();
      odb::dbTechVia* tech_via = tech->findVia(name.c_str());
      if (tech_via != nullptr) {
        _wire_encoder.addTechVia(tech_via);
      } else {
        odb::dbVia* db_via = block->findVia(name.c_str());
        _wire_encoder.addVia(db_via);
      }
    }
    _wire_encoder.end();
  }
}
int ByAdapter::getEndOfLineSpacing(odb::dbTechLayer* layer)
{
  int ans = 0;
  odb::dbSet<odb::dbTechLayerSpacingRule> rules;
  layer->getV54SpacingRules(rules);
  for (auto rule : rules) {
    uint width;
    uint within;
    bool parallelEdge;
    uint parallelSpace;
    uint parallelWithin;
    bool twoEdges;
    bool hs = rule->getEol(
        width, within, parallelEdge, parallelSpace, parallelWithin, twoEdges);
    if (hs) {
      ans = std::max(ans, (int) rule->getSpacing());
    }
  }
  return ans;
}
odb::dbTechLayer* ByAdapter::findNextRoutingLayer(odb::dbTech* dbTech,
                                                  odb::dbTechLayer* curLayer)
{
  bool ok = false;
  for (auto layer : dbTech->getLayers()) {
    if (layer->getType() == odb::dbTechLayerType::ROUTING) {
      if (ok) {
        return layer;
      }
    }
    if (layer == curLayer) {
      ok = true;
    }
  }
  return nullptr;
}
odb::dbTechLayer* ByAdapter::findNextCutLayer(odb::dbTech* dbTech,
                                              odb::dbTechLayer* curLayer)
{
  bool ok = false;
  for (auto layer : dbTech->getLayers()) {
    if (layer->getType() == odb::dbTechLayerType::CUT) {
      if (ok) {
        return layer;
      }
    }
    if (layer == curLayer) {
      ok = true;
    }
  }
  return nullptr;
}

}  // namespace banyan
