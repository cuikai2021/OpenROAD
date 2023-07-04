#include "DrcPatch.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "thread_pool/ThreadPool.h"

namespace banyan {

DrcPatch::~DrcPatch()
{
}

uint getMinEncArea(odb::dbTechLayer* layer)
{
  uint minEncArea = 0;
  std::vector<odb::dbTechMinEncRule*> encRules;
  layer->getMinEnclosureRules(encRules);
  for (auto encRule : encRules) {
    uint area = 0;
    encRule->getEnclosure(area);
    if (minEncArea <= 0 || area < minEncArea) {
      minEncArea = area;
    }
  }
  return minEncArea;
}

std::vector<PlyRect> DrcPatch::sameNetRects(odb::dbTechLayer* layer,
                                            odb::dbNet* net)
{
  std::vector<PlyRect> sameNetRects;

  auto netName = net->getName();
  auto dbWire = net->getWire();
  if (dbWire != nullptr) {
    auto netrects = dbWire2Rects(layer, dbWire);
    sameNetRects.insert(sameNetRects.end(), netrects.begin(), netrects.end());
  }

  for (auto iterm : net->getITerms()) {
    auto inst = iterm->getInst();
    int x, y = 0;
    inst->getOrigin(x, y);
    odb::dbTransform transformer(inst->getOrient(), odb::Point(x, y));
    auto itermName = getItermName(iterm);
    auto mterm = iterm->getMTerm();
    for (auto mpin : mterm->getMPins()) {
      for (auto dbBox : mpin->getGeometry()) {
        if (dbBox->isVia()) {
          if (dbBox->getBlockVia() != nullptr) {
            auto via = dbBox->getBlockVia();
            for (auto viaBox : via->getBoxes()) {
              if (viaBox->getTechLayer()->getName() == layer->getName()) {
                auto rect = dbBox2PlyRect(transformer, viaBox);
                sameNetRects.push_back(rect);
              }
            }
          } else {
            auto via = dbBox->getTechVia();
            for (auto viaBox : via->getBoxes()) {
              if (viaBox->getTechLayer()->getName() == layer->getName()) {
                auto rect = dbBox2PlyRect(transformer, viaBox);
                sameNetRects.push_back(rect);
              }
            }
          }
        } else {
          auto boxLayer = dbBox->getTechLayer();
          if (boxLayer->getName() == layer->getName()) {
            auto rect = dbBox2PlyRect(transformer, dbBox);
            sameNetRects.push_back(rect);
          }
        }
      }
    }
  }

  return sameNetRects;
}

PlyRect DrcPatch::dbBox2PlyRect(const odb::dbTransform& transformer,
                                odb::dbBox* dbBox)
{
  odb::Rect pinBox(dbBox->xMin(), dbBox->yMin(), dbBox->xMax(), dbBox->yMax());
  transformer.apply(pinBox);
  return PlyRect(pinBox.xMin(), pinBox.yMin(), pinBox.xMax(), pinBox.yMax());
}

void DrcPatch::run()
{
  int enableCPUNum = sysconf(_SC_NPROCESSORS_ONLN);
  ThreadPool pool((enableCPUNum - 1) * 2);
  auto start = std::chrono::steady_clock::now();

  auto chip = db_->getChip();
  auto block = chip->getBlock();

  std::vector<std::future<
      std::tuple<odb::dbTechLayer*, odb::dbNet*, std::vector<PlyRect>>>>
      futureRes;
  for (auto layer : db_->getTech()->getLayers()) {
    if (layer->getType() != odb::dbTechLayerType::ROUTING) {
      continue;
    }

    for (auto net : block->getNets()) {
      if (net->isSpecial()) {
        continue;
      }
      futureRes.emplace_back(pool.enqueue(
          [&](odb::dbTechLayer* layer_, odb::dbNet* net_) {
            auto sNetRects = sameNetRects(layer_, net_);
            auto patchRects = spacingDrcPatch(layer_, net_, sNetRects, {});
            auto result = minHolePatch(layer_, net_, sNetRects, patchRects);
            result.insert(result.end(), patchRects.begin(), patchRects.end());
            return std::make_tuple(layer_, net_, std::move(result));
          },
          layer,
          net));
    }
  }
  std::vector<std::tuple<odb::dbTechLayer*, odb::dbNet*, std::vector<PlyRect>>>
      results;
  for (int i = 0; i < futureRes.size(); i++) {
    results.push_back(futureRes[i].get());
  }

  for (auto result : results) {
    auto layer = std::get<0>(result);
    auto net = std::get<1>(result);
    odb::dbWireEncoder _wire_encoder;
    auto dbWire = net->getWire();
    if (dbWire == nullptr) {
      dbWire = odb::dbWire::create(net);
      _wire_encoder.begin(dbWire);
    } else {
      _wire_encoder.append(dbWire);
    }
    auto netPatchRects = std::get<2>(result);
    for (auto rect : netPatchRects) {
      _wire_encoder.newPath(layer, odb::dbWireType("ROUTED"));
      _wire_encoder.addPoint(ply::xl(rect), ply::yl(rect));
      _wire_encoder.addRect(
          0, 0, ply::xh(rect) - ply::xl(rect), ply::yh(rect) - ply::yl(rect));
    }
    _wire_encoder.end();
  }

  auto end = std::chrono::steady_clock::now();
  logger_->info(
      utl::BY,
      1,
      "improve banyan result time: {} ms",
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
          .count());
}

std::vector<PlyRect> DrcPatch::spacingDrcPatch(
    odb::dbTechLayer* layer,
    odb::dbNet* net,
    std::vector<PlyRect> sameNetRects,
    std::vector<PlyRect> patchRects)
{
  auto reqSpacing = layer->getSpacing();

  auto calcPatchs = [&, this](const std::vector<PlyRect>& maxRects) {
    std::vector<PlyRect> plyPatchRects;
    if (maxRects.size() <= 1) {
      return plyPatchRects;
    }
    for (int i = 0; i < maxRects.size() - 1; i++) {
      for (int j = i + 1; j < maxRects.size(); j++) {
        auto rect0 = maxRects[i];
        auto rect1 = maxRects[j];
        if (ply::intersect(rect0, rect1)) {
          continue;
        }

        auto rect0Hor = ply::horizontal(rect0);
        auto rect0Ver = ply::vertical(rect0);
        auto rect1Hor = ply::horizontal(rect1);
        auto rect1Ver = ply::vertical(rect1);
        auto l1dist = ply::manhattan_distance(rect0, rect1);

        if (ply::intersect(rect0Hor, rect1Hor)) {
          if (0 < l1dist && l1dist < reqSpacing) {
            auto minx = std::max(rect0Hor.low(), rect1Hor.low());
            auto maxx = std::min(rect0Hor.high(), rect1Hor.high());
            auto miny = std::min(rect0Ver.high(), rect1Ver.high());
            auto maxy = std::max(rect0Ver.low(), rect1Ver.low());
            plyPatchRects.push_back(PlyRect(minx, miny, maxx, maxy));
          }
        } else if (ply::intersect(rect0Ver, rect1Ver)) {
          if (0 < l1dist && l1dist < reqSpacing) {
            auto minx = std::min(rect0Hor.high(), rect1Hor.high());
            auto maxx = std::max(rect0Hor.low(), rect1Hor.low());
            auto miny = std::max(rect0Ver.low(), rect1Ver.low());
            auto maxy = std::min(rect0Ver.high(), rect1Ver.high());
            plyPatchRects.push_back(PlyRect(minx, miny, maxx, maxy));
          }
        } else {
          if (ply::euclidean_distance(rect0, rect1) < reqSpacing) {
            logger_->info(utl::BY,
                          1,
                          "complex spacing drc between rect0 {} {} {} {}, "
                          "rect1 {} {} {} {}",
                          ply::xl(rect0),
                          ply::yl(rect0),
                          ply::xh(rect0),
                          ply::yh(rect0),
                          ply::xl(rect1),
                          ply::yl(rect1),
                          ply::xh(rect1),
                          ply::yh(rect1));
          }
        }
      }
    }
    return plyPatchRects;
  };

  PlyPolygonSet plyset;
  plyset += sameNetRects;
  plyset += patchRects;

  std::vector<PlyRect> netPatchRects;
  std::vector<PlyRect> allMaxRects;

  std::vector<PlyPolygon> polygons;
  plyset.get_polygons(polygons);
  for (auto polygon : polygons) {
    std::vector<PlyRect> maxRects;
    ply::get_max_rectangles(maxRects, polygon);
    allMaxRects.insert(allMaxRects.end(),
                       std::make_move_iterator(maxRects.begin()),
                       std::make_move_iterator(maxRects.end()));
  }

  auto plyPatchRects = calcPatchs(allMaxRects);
  std::vector<PlyRect> reducedPatchRects;
  ply::get_max_rectangles(reducedPatchRects, plyPatchRects);
  netPatchRects.insert(netPatchRects.end(),
                       std::make_move_iterator(reducedPatchRects.begin()),
                       std::make_move_iterator(reducedPatchRects.end()));
  return netPatchRects;
}
std::vector<PlyRect> DrcPatch::minHolePatch(odb::dbTechLayer* layer,
                                            odb::dbNet* net,
                                            std::vector<PlyRect> sameNetRects,
                                            std::vector<PlyRect> patchRects)
{
  auto minEncArea = getMinEncArea(layer);

  std::vector<PlyRect> netPatchRects;

  PlyPolygonSet plyset;
  auto netName = net->getName();
  plyset += sameNetRects;
  plyset += patchRects;

  std::vector<PlyPolygon> polygons;
  plyset.get(polygons);

  std::vector<PlyPolygonHole> holes;
  for (auto polygon : polygons) {
    for (auto iter = ply::begin_holes(polygon); iter != ply::end_holes(polygon);
         ++iter) {
      holes.push_back(*iter);
    }
  }

  for (auto hole : holes) {
    if (ply::area(hole) < minEncArea) {
      PlyRect bbox;
      ply::extents(bbox, hole);
      netPatchRects.push_back(bbox);
    }
  }

  return netPatchRects;
}

const std::string DrcPatch::getItermName(odb::dbITerm* iterm) const
{
  auto inst = iterm->getInst();
  auto mterm = iterm->getMTerm();
  return inst->getName() + "/" + mterm->getName();
}

std::vector<PlyRect> DrcPatch::dbWire2Rects(odb::dbTechLayer* layer,
                                            odb::dbWire* odbWire)
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();
  auto tech = db_->getTech();
  std::vector<PlyRect> rects;

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

    if (layerName != layer->getName()) {
      return;
    }
    auto width = layer->getWidth();
    auto half = width / 2;
    rects.push_back(PlyRect(x1 - half, y1 - half, x2 + half, y2 + half));
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

    if (layerName != layer->getName()) {
      return;
    }

    rects.push_back(PlyRect(x1, y1, x2, y2));
  };

  auto doAddVia = [&](const std::string& layerName,
                      const std::string& viaName,
                      int x,
                      int y,
                      int ext) {
    auto via = block->findVia(viaName.c_str());
    if (via != nullptr) {
      odb::dbTransform transformer(odb::Point(x, y));
      for (auto viaBox : via->getBoxes()) {
        if (viaBox->getTechLayer()->getName() != layer->getName()) {
          continue;
        }
        rects.push_back(dbBox2PlyRect(transformer, viaBox));
      }
      return;
    }

    auto techVia = tech->findVia(viaName.c_str());
    if (techVia != nullptr) {
      odb::dbTransform transformer(odb::Point(x, y));
      for (auto viaBox : techVia->getBoxes()) {
        if (viaBox->getTechLayer()->getName() != layer->getName()) {
          continue;
        }
        rects.push_back(dbBox2PlyRect(transformer, viaBox));
      }
      return;
    }
  };

  odb::dbWireDecoder decode;
  int prevX = std::numeric_limits<int>::max();
  int prevY = std::numeric_limits<int>::max();
  int prevExt = std::numeric_limits<int>::max();
  bool hasPrevCoor = false;

  for (decode.begin(odbWire);;) {
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
        ext = 0;
        prevX = x;
        prevY = y;
        prevExt = ext;
        hasPrevCoor = true;
        break;
      }

      case odb::dbWireDecoder::POINT: {
        int x, y, ext;
        decode.getPoint(x, y);
        ext = 0;

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
        return rects;

      default:
        break;
    }
  }
  return rects;
}

}  // namespace banyan