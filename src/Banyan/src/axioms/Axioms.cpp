#include "axioms/Axioms.h"

#include <assert.h>
#include <limits.h>

#include <algorithm>
#include <cmath>
#include <list>
#include <map>
#include <utility>

#include "odb/db.h"

namespace banyan {

void Axioms::initDefaultViaMap()
{
  odb::dbTech* tech = db_->getTech();
  const auto techVias = tech->getVias();
  for (auto techVia : techVias) {
    if (techVia->isDefault()) {
      defaultViaMap[techVia->getName()] = techVia;
    }
  }
}

void Axioms::getDefaultViasInLayer(
    odb::dbTechLayer* layer,
    std::vector<std::pair<std::string, odb::dbTechVia*>>& defaultViasInLayer)
{
  /* def vias seem missing
  auto la = AssertFind(dbAdapter_.layerNameToIdxs_, layer->getName());
  if (la < 0)
    throw odb::ZException("invalid viadef layers");

  for (auto i = dbAdapter_.layers_[la].viadef_begin;
    i != dbAdapter_.layers_[la].viadef_end; ++i) {
      const auto &viadef = dbAdapter_.viadefs_[i];
      std::string name(viadef.name);
      auto via = AssertFind(defaultViaMap, name);
      defaultViasInLayer.emplace_back(name, via);
  }
  if (size_t(la+1) < dbAdapter_.layers_.size()) {
    for (auto i = dbAdapter_.layers_[la+1].viadef_begin;
      i != dbAdapter_.layers_[la+1].viadef_end; ++i) {
        const auto &viadef = dbAdapter_.viadefs_[i];
        std::string name(viadef.name);
        auto via = AssertFind(defaultViaMap, name);
        defaultViasInLayer.emplace_back(name, via);
    }
  }
  //*/
  //*
  copy_if(defaultViaMap.begin(),
          defaultViaMap.end(),
          std::inserter(defaultViasInLayer, defaultViasInLayer.end()),
          [layer](auto const& kv_pair) {
            auto via = kv_pair.second;
            return via->getTopLayer()->getName() == layer->getName()
                   || via->getBottomLayer()->getName() == layer->getName();
          });
  //*/
}

uint32_t Axioms::getCutNumsFromVia(odb::dbTechVia* via)
{
  if (via == nullptr) {
    return 0;
  }
  uint32_t num;
  for (auto box : via->getBoxes()) {
    if (box->getTechLayer()->getType().getValue()
        != odb::dbTechLayerType::CUT) {
      continue;
    }
    num++;
  }
  return num;
}

void Axioms::getBoundaryBoxOfCutBoxesFromVia(odb::dbTechVia* via,
                                             odb::Rect* resultBox)
{
  if (via == nullptr || resultBox == nullptr) {
    return;
  }

  for (auto box : via->getBoxes()) {
    if (box->getTechLayer()->getType().getValue()
        != odb::dbTechLayerType::CUT) {
      continue;
    }
    if (box->xMin() < resultBox->xMin()) {
      resultBox->set_xlo(box->xMin());
    }
    if (box->yMin() < resultBox->yMin()) {
      resultBox->set_ylo(box->yMin());
    }
    if (box->xMax() > resultBox->xMax()) {
      resultBox->set_xhi(box->xMax());
    }
    if (box->yMax() > resultBox->yMax()) {
      resultBox->set_yhi(box->yMax());
    }
  }

  return;
}

odb::dbBox* Axioms::getLayerBoxFromVia(odb::dbTechLayer* layer,
                                       odb::dbTechVia* via)
{
  if (via == nullptr) {
    return nullptr;
  }
  for (auto box : via->getBoxes()) {
    if (box->getTechLayer() != nullptr
        && box->getTechLayer()->getName() == layer->getName()) {
      return box;
    }
  }
  return nullptr;
}

odb::dbTechLayer* Axioms::getCutLayerFromVia(odb::dbTechVia* via)
{
  if (via == nullptr) {
    return nullptr;
  }
  odb::dbTechLayer* cutLayer = nullptr;
  for (auto box : via->getBoxes()) {
    if (box->getTechLayer()->getType().getValue()
        == odb::dbTechLayerType::CUT) {
      cutLayer = box->getTechLayer();
      break;
    }
  }
  return cutLayer;
}

uint64_t Axioms::getRoutingPRLSpacing(odb::dbTechLayer* layer, bool isSameNet)
{
  if (layer == nullptr) {
    return 0;
  }
  uint spacing = layer->getSpacing();

  if (layer->hasV55SpacingRules()) {
    spacing = std::max(layer->findV55Spacing((int) layer->getWidth(), INT_MAX),
                       layer->getSpacing());
  }

  odb::dbSet<odb::dbTechLayerSpacingRule> spacingRules;
  layer->getV54SpacingRules(spacingRules);
  for (auto spacingRule : spacingRules) {
    uint width = 0;
    uint within = 0;
    bool parallelEdge = false;
    uint parallelSpace = 0;
    uint parallelWithin = 0;
    bool twoEdges = false;
    if (spacingRule->getEol(width,
                            within,
                            parallelEdge,
                            parallelSpace,
                            parallelWithin,
                            twoEdges)) {
      continue;
    } else if (spacingRule->getSpacing() > spacing) {
      spacing = spacingRule->getSpacing();
    }
  }

  return spacing;
}

uint64_t Axioms::getRoutingEOLSpacing(odb::dbTechLayer* layer, bool isSameNet)
{
  if (layer == nullptr) {
    return 0;
  }
  uint spacing = layer->getSpacing();

  auto prlSpacing = getRoutingPRLSpacing(layer, isSameNet);

  odb::dbSet<odb::dbTechLayerSpacingRule> spacingRules;
  layer->getV54SpacingRules(spacingRules);

  for (auto spacingRule : spacingRules) {
    uint eolWidth = 0;
    uint eolWithin = 0;
    bool parallelEdge = false;
    uint parallelSpace = 0;
    uint parallelWithin = 0;
    bool twoEdges = false;
    if (spacingRule->getEol(eolWidth,
                            eolWithin,
                            parallelEdge,
                            parallelSpace,
                            parallelWithin,
                            twoEdges)) {
      if (layer->getWidth() > eolWidth) {
        continue;
      }
      if (parallelSpace == 0 || parallelWithin == 0) {
         spacing = spacingRule->getSpacing();
         break;
      } else {
        if (prlSpacing < parallelWithin
            || (uint) layer->getSpacing() < eolWithin) {
          spacing = spacingRule->getSpacing();
          break;
        }
      }
    } else if (spacingRule->getSpacing() > spacing) {
      spacing = spacingRule->getSpacing();
    }
  }

  return spacing;
}

uint64_t Axioms::getCutSpacingFromVia(odb::dbTechVia* viaX,
                                      odb::dbTechVia* viaY,
                                      bool isSameNet)
{
  if (viaX == nullptr || viaY == nullptr) {
    return 0;
  }
  odb::dbTechLayer* cutLayerForViaX = getCutLayerFromVia(viaX);
  odb::dbTechLayer* cutLayerForViaY = getCutLayerFromVia(viaY);
  if (cutLayerForViaX->getName() != cutLayerForViaY->getName()) {
    return 0;
  }
  uint cutSpacing = 0;

  odb::dbSet<odb::dbTechLayerSpacingRule> spacingRules;
  cutLayerForViaX->getV54SpacingRules(spacingRules);

  for (auto spacingRule : spacingRules) {
    // skip parallelOverlap cut spacing rule
    if (spacingRule->getCutParallelOverlap()) {
      continue;
    }
    uint numcuts = 0;
    uint within = 0;
    uint spacing = 0;
    bool except_same_pgnet = false;
    // skip adjacent cut spacing rule
    if (spacingRule->getAdjacentCuts(
            numcuts, within, spacing, except_same_pgnet)) {
      continue;
    }

    if (!isSameNet && !spacingRule->getCutSameNet()) {
      if (spacingRule->getSpacing() > cutSpacing) {
        cutSpacing = spacingRule->getSpacing();
      }
    } else if (isSameNet && spacingRule->getCutSameNet()) {
      cutSpacing = spacingRule->getSpacing();
      break;
    } else if (isSameNet) {
      cutSpacing = spacingRule->getSpacing();
    }
  }

  if (cutSpacing <= 0) {
    cutSpacing = cutLayerForViaX->getSpacing();
  }

  return (uint64_t) cutSpacing;
}

uint64_t Axioms::getDiffAreaBetweenViaAndWire(odb::dbBox* metalBoxOfVia,
                                              uint64_t width,
                                              bool isHorizontal)
{
  if (metalBoxOfVia == nullptr) {
    return 0;
  }
  uint64_t diffArea = 0;
  uint64_t diffLenOfBoxOfVia = 0;
  uint64_t metalBoxOfViaWidth
      = (uint64_t) (dbAdapter_.defdist(metalBoxOfVia->getDX()));
  uint64_t metalBoxOfViaHeight
      = (uint64_t) (dbAdapter_.defdist(metalBoxOfVia->getDY()));
  if (isHorizontal) {
    if (metalBoxOfViaHeight > width) {
      diffLenOfBoxOfVia = metalBoxOfViaHeight - width;
      diffArea = diffLenOfBoxOfVia * metalBoxOfViaWidth;
    }
  } else {
    if (metalBoxOfViaWidth > width) {
      diffLenOfBoxOfVia = metalBoxOfViaWidth - width;
      diffArea = diffLenOfBoxOfVia * metalBoxOfViaHeight;
    }
  }
  return diffArea;
}

uint64_t Axioms::getMarginBetweenCutAndMetalBox(odb::dbBox* metalBox,
                                                odb::Rect* cutBox,
                                                POSITION position)
{
  if (metalBox == nullptr || cutBox == nullptr) {
    return 0;
  }

  uint64_t margin = 0;

  switch (position) {
    case POSITION::LEFT:
      margin = abs(metalBox->xMin() - cutBox->xMin());
      break;
    case POSITION::BOTTOM:
      margin = abs(metalBox->yMin() - cutBox->yMin());
      break;
    case POSITION::RIGHT:
      margin = abs(metalBox->xMax() - cutBox->xMax());
      break;
    case POSITION::TOP:
      margin = abs(metalBox->yMax() - cutBox->yMax());
      break;
  }

  return dbAdapter_.defdist(margin);
}

uint64_t Axioms::calcViaViaTrunkLength(odb::dbTechLayer* techLayer,
                                       odb::dbTechVia* viaX,
                                       odb::dbTechVia* viaY,
                                       bool isSameNet)
{
  if (viaX == nullptr || viaY == nullptr) {
    return 0;
  }
  uint64_t minLen = 0;
  auto width = techLayer->getWidth();
  auto layerDir = techLayer->getDirection();
  uint64_t cutSpacing = getCutSpacingFromVia(viaX, viaY, isSameNet);

  auto metalBoxOfViaX = getLayerBoxFromVia(techLayer, viaX);
  auto metalBoxOfViaY = getLayerBoxFromVia(techLayer, viaY);

  uint64_t borderBoxOfViaXCenterToEdge = 0;
  uint64_t borderBoxOfViaYCenterToEdge = 0;

  uint64_t diffLenOfBoxOfViaX = 0;
  uint64_t diffLenOfBoxOfViaY = 0;

  // uint32_t cutNumsOfViaX = getCutNumsFromVia(viaX);
  // uint32_t cutNumsOfViaY = getCutNumsFromVia(viaY);

  uint32_t borderCutBoxOfViaXToMetalBoxEdge = 0;
  uint32_t borderCutBoxOfViaYToMetalBoxEdge = 0;

  odb::Rect* boundaryBoxOfViaX = new odb::Rect();
  odb::Rect* boundaryBoxOfViaY = new odb::Rect();

  getBoundaryBoxOfCutBoxesFromVia(viaX, boundaryBoxOfViaX);
  getBoundaryBoxOfCutBoxesFromVia(viaY, boundaryBoxOfViaY);

  if (layerDir == odb::dbTechLayerDir::HORIZONTAL) {
    if (viaX != nullptr) {
      borderBoxOfViaXCenterToEdge
          = dbAdapter_.defdist(boundaryBoxOfViaX->dx() / 2);
      borderCutBoxOfViaXToMetalBoxEdge = getMarginBetweenCutAndMetalBox(
          metalBoxOfViaX, boundaryBoxOfViaX, POSITION::RIGHT);
      uint64_t metalBoxOfViaXHeight
          = (uint64_t) (dbAdapter_.defdist(metalBoxOfViaX->getDY()));
      if (metalBoxOfViaXHeight > width) {
        diffLenOfBoxOfViaX = metalBoxOfViaXHeight - width;
      }
    }

    if (viaY != nullptr) {
      borderBoxOfViaYCenterToEdge
          = dbAdapter_.defdist(boundaryBoxOfViaY->dx() / 2);
      borderCutBoxOfViaYToMetalBoxEdge = getMarginBetweenCutAndMetalBox(
          metalBoxOfViaX, boundaryBoxOfViaX, POSITION::LEFT);
      uint64_t metalBoxOfViaYHeight
          = (uint64_t) (dbAdapter_.defdist(metalBoxOfViaY->getDY()));
      if (metalBoxOfViaYHeight > width) {
        diffLenOfBoxOfViaY = metalBoxOfViaYHeight - width;
      }
    }

  } else {
    if (viaX != nullptr) {
      borderBoxOfViaXCenterToEdge
          = dbAdapter_.defdist(boundaryBoxOfViaX->dy() / 2);
      borderCutBoxOfViaXToMetalBoxEdge = getMarginBetweenCutAndMetalBox(
          metalBoxOfViaX, boundaryBoxOfViaX, POSITION::TOP);
      uint64_t metalBoxOfViaXWidth
          = (uint64_t) (dbAdapter_.defdist(metalBoxOfViaX->getDX()));
      if (metalBoxOfViaXWidth > width) {
        diffLenOfBoxOfViaX = metalBoxOfViaXWidth - width;
      }
    }

    if (viaY != nullptr) {
      borderBoxOfViaYCenterToEdge
          = dbAdapter_.defdist(boundaryBoxOfViaY->dy() / 2);
      borderCutBoxOfViaYToMetalBoxEdge = getMarginBetweenCutAndMetalBox(
          metalBoxOfViaX, boundaryBoxOfViaX, POSITION::BOTTOM);
      uint64_t metalBoxOfViaYWidth
          = (uint64_t) (dbAdapter_.defdist(metalBoxOfViaY->getDX()));
      if (metalBoxOfViaYWidth > width) {
        diffLenOfBoxOfViaY = metalBoxOfViaYWidth - width;
      }
    }
  }

  auto minStep = techLayer->getMinStep();
  // auto minStepMaxEdges = techLayer->getMinStepMaxEdges();
  // auto minStepMaxLength = techLayer->getMinStepMaxLength();

  // if (minStep > 0 && diffLenOfBoxOfViaX > 0 && diffLenOfBoxOfViaY > 0
  //     && diffLenOfBoxOfViaX < minStep && diffLenOfBoxOfViaY < minStep
  //     && (cutSpacing - borderCutBoxOfViaXToMetalBoxEdge
  //         - borderCutBoxOfViaYToMetalBoxEdge)
  //            < minStep) {
  //   cutSpacing += minStep + borderCutBoxOfViaXToMetalBoxEdge
  //                 + borderCutBoxOfViaYToMetalBoxEdge - cutSpacing;
  // }
  if (minStep > 0 && diffLenOfBoxOfViaX > 0 && diffLenOfBoxOfViaY > 0
      && (cutSpacing - borderCutBoxOfViaXToMetalBoxEdge
          - borderCutBoxOfViaYToMetalBoxEdge)
             < minStep) {
    cutSpacing = minStep + borderCutBoxOfViaXToMetalBoxEdge
                 + borderCutBoxOfViaYToMetalBoxEdge;
  }

  minLen
      = borderBoxOfViaXCenterToEdge + borderBoxOfViaYCenterToEdge + cutSpacing;

  delete boundaryBoxOfViaX;
  delete boundaryBoxOfViaY;

  return minLen;
}

uint64_t Axioms::calcTrunkMinLength(odb::dbTechLayer* techLayer,
                                    odb::dbTechVia* viaX,
                                    odb::dbTechVia* viaY,
                                    bool isSameNet)
{
  uint64_t width = techLayer->getWidth();
  auto layerDir = techLayer->getDirection();
  auto area = dbAdapter_.defArea(techLayer->getArea());

  uint64_t minLen = calcViaViaTrunkLength(techLayer, viaX, viaY, isSameNet);

  uint64_t diffAreaOfBoxOfViaX = 0;
  uint64_t diffAreaOfBoxOfViaY = 0;

  uint64_t metalBoxWidthOfViaX = 0;
  uint64_t metalBoxWidthOfViaY = 0;

  // uint32_t cutNumsOfViaX = getCutNumsFromVia(viaX);
  // uint32_t cutNumsOfViaY = getCutNumsFromVia(viaY);

  auto isHorizontal = layerDir == odb::dbTechLayerDir::HORIZONTAL;
  if (viaX != nullptr) {
    auto metalBoxOfViaX = getLayerBoxFromVia(techLayer, viaX);
    diffAreaOfBoxOfViaX
        = getDiffAreaBetweenViaAndWire(metalBoxOfViaX, width, isHorizontal);
    metalBoxWidthOfViaX = dbAdapter_.defdist(
        isHorizontal ? metalBoxOfViaX->getDX() : metalBoxOfViaX->getDY());
  }

  if (viaY != nullptr) {
    auto metalBoxOfViaY = getLayerBoxFromVia(techLayer, viaY);
    diffAreaOfBoxOfViaY
        = getDiffAreaBetweenViaAndWire(metalBoxOfViaY, width, isHorizontal);
    metalBoxWidthOfViaY = dbAdapter_.defdist(
        isHorizontal ? metalBoxOfViaY->getDX() : metalBoxOfViaY->getDY());
  }

  int64_t remainArea = area - diffAreaOfBoxOfViaX - diffAreaOfBoxOfViaY;
  uint64_t realLength = minLen + std::max(metalBoxWidthOfViaX, width) / 2
                        + std::max(metalBoxWidthOfViaY, width) / 2;
  uint64_t realArea = realLength * width;

  if (remainArea > 0 && remainArea > (int64_t) realArea) {
    uint64_t extLen = (remainArea - realArea) / width;
    minLen += extLen;
  }

  return minLen;
}

uint64_t Axioms::calcViaTrackLength(odb::dbTechLayer* techLayer,
                                    odb::dbTechVia* via)
{
  auto layerDir = techLayer->getDirection();
  auto isHorizontal = layerDir == odb::dbTechLayerDir::HORIZONTAL;

  // uint64_t spacing = getCutSpacingFromVia(via, via, false);
  // auto metalBoxOfVia = getLayerBoxFromVia(techLayer, via);
  // odb::Rect* boundaryBoxOfVia = new odb::Rect();
  // getBoundaryBoxOfCutBoxesFromVia(via, boundaryBoxOfVia);

  // if (layerDir == odb::dbTechLayerDir::HORIZONTAL) {
  //    spacing -= getMarginBetweenCutAndMetalBox(metalBoxOfVia,
  //    boundaryBoxOfVia, POSITION::TOP);
  // } else {
  //    spacing -= getMarginBetweenCutAndMetalBox(metalBoxOfVia,
  //    boundaryBoxOfVia, POSITION::RIGHT);
  // }

  // delete boundaryBoxOfVia;

  // return std::max(spacing, getRoutingPRLSpacing(techLayer, false));
  uint64_t width = techLayer->getWidth();
  auto metalBoxOfVia = getLayerBoxFromVia(techLayer, via);
  auto prlSpacing = getRoutingPRLSpacing(techLayer, false);
  uint64_t boxWidth = dbAdapter_.defdist(isHorizontal ? metalBoxOfVia->getDY()
                                                      : metalBoxOfVia->getDX());

  return std::max(boxWidth, width) / 2 + width / 2 + prlSpacing;
}

uint64_t Axioms::calcTrunkDistLength(odb::dbTechLayer* techLayer,
                                     odb::dbTechVia* viaX,
                                     odb::dbTechVia* viaY,
                                     bool isSameNet)
{
  auto layerDir = techLayer->getDirection();
  auto isHorizontal = layerDir == odb::dbTechLayerDir::HORIZONTAL;
  uint64_t width = techLayer->getWidth();

  auto metalBoxOfViaX = getLayerBoxFromVia(techLayer, viaX);
  auto metalBoxOfViaY = getLayerBoxFromVia(techLayer, viaY);

  uint64_t metalBoxWidthOfViaX = 0;
  uint64_t metalBoxWidthOfViaY = 0;

  odb::Rect* boundaryBoxOfViaX = new odb::Rect();
  odb::Rect* boundaryBoxOfViaY = new odb::Rect();
  getBoundaryBoxOfCutBoxesFromVia(viaX, boundaryBoxOfViaX);
  getBoundaryBoxOfCutBoxesFromVia(viaY, boundaryBoxOfViaY);

  int cutSpacing = 0;
  if (viaX != nullptr && viaY != nullptr) {
    cutSpacing = getCutSpacingFromVia(viaX, viaY, isSameNet);
    if (isHorizontal) {
      cutSpacing -= getMarginBetweenCutAndMetalBox(
          metalBoxOfViaX, boundaryBoxOfViaX, POSITION::RIGHT);
      cutSpacing -= getMarginBetweenCutAndMetalBox(
          metalBoxOfViaY, boundaryBoxOfViaY, POSITION::LEFT);
    } else {
      cutSpacing -= getMarginBetweenCutAndMetalBox(
          metalBoxOfViaX, boundaryBoxOfViaX, POSITION::TOP);
      cutSpacing -= getMarginBetweenCutAndMetalBox(
          metalBoxOfViaY, boundaryBoxOfViaY, POSITION::BOTTOM);
    }
  }

  if (metalBoxOfViaX != nullptr) {
    metalBoxWidthOfViaX = dbAdapter_.defdist(
        isHorizontal ? metalBoxOfViaX->getDX() : metalBoxOfViaX->getDY());
  }

  if (metalBoxOfViaY != nullptr) {
    metalBoxWidthOfViaY = dbAdapter_.defdist(
        isHorizontal ? metalBoxOfViaY->getDX() : metalBoxOfViaY->getDY());
  }

  auto routingSpacing
      = dbAdapter_.defdist(getRoutingEOLSpacing(techLayer, isSameNet));


  uint64_t spacing = std::max(routingSpacing, cutSpacing);

  delete boundaryBoxOfViaX;
  delete boundaryBoxOfViaY;

  return spacing + std::max(metalBoxWidthOfViaX, width) / 2
         + std::max(metalBoxWidthOfViaY, width) / 2;
}

uint64_t Axioms::calcViaViaTrackLength(odb::dbTechLayer* techLayer,
                                       odb::dbTechVia* viaX,
                                       odb::dbTechVia* viaY,
                                       bool isSameNet)
{
  if (techLayer == nullptr || viaX == nullptr || viaY == nullptr) {
    return 0;
  }
  auto layerDir = techLayer->getDirection();
  auto isHorizontal = layerDir == odb::dbTechLayerDir::HORIZONTAL;
  uint64_t width = techLayer->getWidth();

  auto metalBoxOfViaX = getLayerBoxFromVia(techLayer, viaX);
  auto metalBoxOfViaY = getLayerBoxFromVia(techLayer, viaY);

  odb::Rect* boundaryBoxOfViaX = new odb::Rect();
  odb::Rect* boundaryBoxOfViaY = new odb::Rect();

  getBoundaryBoxOfCutBoxesFromVia(viaX, boundaryBoxOfViaX);
  getBoundaryBoxOfCutBoxesFromVia(viaY, boundaryBoxOfViaY);

  uint cutSpacing = getCutSpacingFromVia(viaX, viaY, isSameNet);
  uint marginBetweenCutsAndMetalBox = 0;
  uint cutsCenterToCenter = 0;
  if (isHorizontal) {
    cutsCenterToCenter
        = boundaryBoxOfViaX->dx() / 2 + boundaryBoxOfViaY->dx() / 2;
    marginBetweenCutsAndMetalBox
        = getMarginBetweenCutAndMetalBox(
              metalBoxOfViaX, boundaryBoxOfViaX, POSITION::TOP)
          + getMarginBetweenCutAndMetalBox(
              metalBoxOfViaY, boundaryBoxOfViaY, POSITION::BOTTOM);
  } else {
    cutsCenterToCenter
        = boundaryBoxOfViaX->dy() / 2 + boundaryBoxOfViaY->dy() / 2;
    marginBetweenCutsAndMetalBox
        = getMarginBetweenCutAndMetalBox(
              metalBoxOfViaX, boundaryBoxOfViaX, POSITION::RIGHT)
          + getMarginBetweenCutAndMetalBox(
              metalBoxOfViaY, boundaryBoxOfViaY, POSITION::LEFT);
  }

  delete boundaryBoxOfViaX;
  delete boundaryBoxOfViaY;

  if (cutSpacing <= width + marginBetweenCutsAndMetalBox) {
    return cutsCenterToCenter;
  }

  double horizontalSpacing
      = std::sqrt(std::pow(cutSpacing, 2)
                  - std::pow(width + marginBetweenCutsAndMetalBox, 2));

  horizontalSpacing = std::ceil(horizontalSpacing / 10) * 10;

  return horizontalSpacing + cutsCenterToCenter;
}

void Axioms::addTableTriples(
    odb::dbTechLayer* techLayer,
    std::vector<std::pair<std::string, odb::dbTechVia*>> defaultViasInLayer,
    CalcLength calcFunc,
    bool isSameNet,
    bool excludedNullVia)
{
  if (!excludedNullVia) {
    tableTriples.push_back(
        {NULL_VIA_IDX,
         NULL_VIA_IDX,
         (this->*calcFunc)(techLayer, nullptr, nullptr, isSameNet)});

    for (const auto& nameToViaX : defaultViasInLayer) {
      auto viaXIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaX.first);
      auto viaX = nameToViaX.second;
      tableTriples.push_back(
          {viaXIdx,
           NULL_VIA_IDX,
           (this->*calcFunc)(techLayer, viaX, nullptr, isSameNet)});
    }
    for (const auto& nameToViaY : defaultViasInLayer) {
      auto viaYIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaY.first);
      auto viaY = nameToViaY.second;
      tableTriples.push_back(
          {NULL_VIA_IDX,
           viaYIdx,
           (this->*calcFunc)(techLayer, nullptr, viaY, isSameNet)});
    }
  }

  for (const auto& nameToViaX : defaultViasInLayer) {
    auto viaXIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaX.first);
    auto viaX = nameToViaX.second;
    for (const auto& nameToViaY : defaultViasInLayer) {
      auto viaYIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaY.first);
      auto viaY = nameToViaY.second;
      tableTriples.push_back(
          {viaXIdx,
           viaYIdx,
           (this->*calcFunc)(techLayer, viaX, viaY, isSameNet)});
    }
  }
}

void Axioms::initTable()
{
  odb::dbTech* tech = db_->getTech();
  for (auto techLayer : tech->getLayers()) {
    if (techLayer->getType().getValue() != odb::dbTechLayerType::ROUTING) {
      continue;
    }
    int layerIdx
        = AssertFind(dbAdapter_.layerNameToIdxs_, techLayer->getName());
    std::vector<std::pair<std::string, odb::dbTechVia*>> defaultViasInLayer;
    getDefaultViasInLayer(techLayer, defaultViasInLayer);
    //===============================================================================================
    dbAdapter_.layers_[layerIdx].trunk_min_len_begin = tableTriples.size();
    addTableTriples(
        techLayer, defaultViasInLayer, &Axioms::calcTrunkMinLength, false);
    dbAdapter_.layers_[layerIdx].trunk_min_len_end = tableTriples.size();
    //===============================================================================================
    dbAdapter_.layers_[layerIdx].trunk_dist_same_begin = tableTriples.size();
    addTableTriples(
        techLayer, defaultViasInLayer, &Axioms::calcTrunkDistLength, true);
    dbAdapter_.layers_[layerIdx].trunk_dist_same_end = tableTriples.size();
    //===============================================================================================
    dbAdapter_.layers_[layerIdx].trunk_dist_diff_begin = tableTriples.size();
    addTableTriples(
        techLayer, defaultViasInLayer, &Axioms::calcTrunkDistLength, false);
    dbAdapter_.layers_[layerIdx].trunk_dist_diff_end = tableTriples.size();
    //===============================================================================================
    dbAdapter_.layers_[layerIdx].viavia_trunk_begin = tableTriples.size();
    addTableTriples(techLayer,
                    defaultViasInLayer,
                    &Axioms::calcViaViaTrunkLength,
                    false,
                    true);
    dbAdapter_.layers_[layerIdx].viavia_trunk_end = tableTriples.size();
    //===============================================================================================
    dbAdapter_.layers_[layerIdx].viavia_track_same_begin = tableTriples.size();
    addTableTriples(techLayer,
                    defaultViasInLayer,
                    &Axioms::calcViaViaTrackLength,
                    true,
                    true);
    dbAdapter_.layers_[layerIdx].viavia_track_same_end = tableTriples.size();
    //===============================================================================================
    dbAdapter_.layers_[layerIdx].viavia_track_diff_begin = tableTriples.size();
    addTableTriples(techLayer,
                    defaultViasInLayer,
                    &Axioms::calcViaViaTrackLength,
                    false,
                    true);
    dbAdapter_.layers_[layerIdx].viavia_track_diff_end = tableTriples.size();
    //===============================================================================================
    dbAdapter_.layers_[layerIdx].via_track_begin = tableTriples.size();
    for (auto viaIter : defaultViasInLayer) {
      auto viaIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, viaIter.first);
      auto via = viaIter.second;
      tableTriples.push_back(
          {viaIdx, NULL_VIA_IDX, calcViaTrackLength(techLayer, via)});
    }
    dbAdapter_.layers_[layerIdx].via_track_end = tableTriples.size();
    //================================================================================================
    dbAdapter_.layers_[layerIdx].spacing_begin = tableTriples.size();
    std::vector<uint> width_idx;
    std::vector<uint> length_idx;
    if (techLayer->hasV55SpacingRules()
        && techLayer->getV55SpacingWidthsAndLengths(width_idx, length_idx)) {
      for (auto width : width_idx) {
        for (auto prl : length_idx) {
          // for width and prl 'greater than'
          auto spacing = techLayer->findV55Spacing(width + 1, prl + 1);
          tableTriples.push_back({(int) prl, (int) width, (uint64) spacing});
        }
      }
    }
    dbAdapter_.layers_[layerIdx].spacing_end = tableTriples.size();
    //================================================================================================
    dbAdapter_.layers_[layerIdx].mincut_begin = tableTriples.size();
    std::vector<odb::dbTechMinCutRule*> cut_rules;
    if (techLayer->getMinimumCutRules(cut_rules)) {
      uint width;
      uint numcuts;
      uint length;
      uint distance;
      for (auto mincut_rule : cut_rules) {
        if (mincut_rule->getMinimumCuts(numcuts, width)) {
          if (mincut_rule->getLengthForCuts(length, distance)) {
            tableTriples.push_back(
                {(int) width, (int) distance, (uint64) numcuts});
          } else {
            tableTriples.push_back({(int) width, 0, (uint64) numcuts});
          }
        }
      }
    }
    dbAdapter_.layers_[layerIdx].mincut_end = tableTriples.size();
  }
}

}  // namespace banyan
