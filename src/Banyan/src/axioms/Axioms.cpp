#include "axioms/Axioms.h"

#include <assert.h>
#include <limits.h>

#include <algorithm>
#include <cmath>
#include <list>
#include <map>
#include <utility>

#include "opendb/db.h"

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
  copy_if(defaultViaMap.begin(),
          defaultViaMap.end(),
          std::inserter(defaultViasInLayer, defaultViasInLayer.end()),
          [layer](auto const& kv_pair) {
            auto via = kv_pair.second;
            return via->getTopLayer()->getName() == layer->getName()
                   || via->getBottomLayer()->getName() == layer->getName();
          });
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
  uint64_t spacing = 0;
  if (layer->hasV55SpacingRules()) {
    spacing = layer->findV55Spacing(layer->getWidth(), INT_MAX);
  } else {
    odb::dbSet<odb::dbTechLayerSpacingRule> spacingRules;
    layer->getV54SpacingRules(spacingRules);

    for (auto spacingRule : spacingRules) {
      if ((isSameNet && spacingRule->getCutSameNet() && !spacingRule->getSameNetPgOnly()) || (!isSameNet && !spacingRule->getCutSameNet())) {

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
    }
  }

  if (spacing <= 0) {
    spacing = layer->getSpacing();
  }

  return spacing;
}

uint64_t Axioms::getRoutingEOLSpacing(odb::dbTechLayer* layer, bool isSameNet)
{
  if (layer == nullptr) {
    return 0;
  }
  uint64_t spacing = 0;

  odb::dbSet<odb::dbTechLayerSpacingRule> spacingRules;
  layer->getV54SpacingRules(spacingRules);

  for (auto spacingRule : spacingRules) {
    if ((isSameNet && spacingRule->getCutSameNet()
         && !spacingRule->getSameNetPgOnly())
        || (!isSameNet && !spacingRule->getCutSameNet())) {
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
        if (width > layer->getWidth()) {
          spacing = spacingRule->getSpacing();
        }
      } else if (spacingRule->getSpacing() > spacing) {
        spacing = spacingRule->getSpacing();
      }
    }
  }

  if (spacing <= 0) {
    spacing = layer->getSpacing();
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
  int cutSpacing = 0;

  odb::dbSet<odb::dbTechLayerSpacingRule> spacingRules;
  cutLayerForViaX->getV54SpacingRules(spacingRules);

  for (auto spacingRule : spacingRules) {
    if ((isSameNet && spacingRule->getCutSameNet()
         && !spacingRule->getSameNetPgOnly())
        || (!isSameNet && !spacingRule->getCutSameNet())) {
      if (spacingRule->getSpacing() > cutSpacing) {
        cutSpacing = spacingRule->getSpacing();
      }
    }
  }

  // for (auto cutSpacingRule : cutLayerForViaX->getTechLayerCutSpacingRules())
  // {
  //   if (cutSpacingRule->isExceptSameNet() ||
  //   cutSpacingRule->isExceptSameMetal()
  //       || cutSpacingRule->isSameVia()) {
  //     continue;
  //   }
  //   if (cutSpacingRule->getCutSpacing() > cutSpacing) {
  //     cutSpacing = cutSpacingRule->getCutSpacing();
  //   }
  // }

  // for (auto cutSpacingRule : cutLayerForViaY->getTechLayerCutSpacingRules())
  // {
  //   if (cutSpacingRule->isExceptSameNet() ||
  //   cutSpacingRule->isExceptSameMetal()
  //       || cutSpacingRule->isSameVia()) {
  //     continue;
  //   }
  //   if (cutSpacingRule->getCutSpacing() > cutSpacing) {
  //     cutSpacing = cutSpacingRule->getCutSpacing();
  //   }
  // }
  if (cutSpacing <= 0) {
    cutSpacing = cutLayerForViaX->getSpacing();
  }

  return (uint64_t) cutSpacing;
}


uint64_t Axioms::getDiffAreaBetweenViaAndWire(odb::dbBox* metalBoxOfVia, uint64_t width, bool isHorizontal)
{
  if(metalBoxOfVia == nullptr){
    return 0;
  }
  uint64_t diffArea = 0;
  uint64_t diffLenOfBoxOfVia = 0;
  if (isHorizontal) {
    if (dbAdapter_.defdist(metalBoxOfVia->getDY()) > width) {
      diffLenOfBoxOfVia = dbAdapter_.defdist(metalBoxOfVia->getDY()) - width;
      diffArea = diffLenOfBoxOfVia * dbAdapter_.defdist(metalBoxOfVia->getDX());
    }
  } else {
    if (dbAdapter_.defdist(metalBoxOfVia->getDX()) > width) {
      diffLenOfBoxOfVia = dbAdapter_.defdist(metalBoxOfVia->getDX()) - width;
      diffArea = diffLenOfBoxOfVia * dbAdapter_.defdist(metalBoxOfVia->getDY());
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

uint64_t Axioms::calcTrunkMinLength(odb::dbTechLayer* techLayer,
                                    odb::dbTechVia* viaX,
                                    odb::dbTechVia* viaY,
                                    bool isSameNet)
{
  auto width = techLayer->getWidth();
  auto layerDir = techLayer->getDirection();
  auto area = dbAdapter_.defArea(techLayer->getArea());

  uint64_t minLen = calcViaTrunkLength(techLayer, viaX, viaY, isSameNet);

  uint64_t diffAreaOfBoxOfViaX = 0;
  uint64_t diffAreaOfBoxOfViaY = 0;

  // uint32_t cutNumsOfViaX = getCutNumsFromVia(viaX);
  // uint32_t cutNumsOfViaY = getCutNumsFromVia(viaY);

  auto isHorizontal = layerDir == odb::dbTechLayerDir::HORIZONTAL;
  if (viaX != nullptr) {
    auto metalBoxOfViaX = getLayerBoxFromVia(techLayer, viaX);
    diffAreaOfBoxOfViaX
        = getDiffAreaBetweenViaAndWire(metalBoxOfViaX, width, isHorizontal);
  }
  if (viaY != nullptr) {
    auto metalBoxOfViaY = getLayerBoxFromVia(techLayer, viaY);
    diffAreaOfBoxOfViaY
        = getDiffAreaBetweenViaAndWire(metalBoxOfViaY, width, isHorizontal);
  }

  int64_t remainArea = area - diffAreaOfBoxOfViaX - diffAreaOfBoxOfViaY;
  if (remainArea > 0 && minLen * width < remainArea) {
    uint64_t extLen = (remainArea - minLen * width) / width;
    minLen += extLen;
  }


  // std::string viaXName = viaX == nullptr ? "null" : viaX->getName();
  // std::string viaYName = viaY == nullptr ? "null" : viaY->getName();

  // logger_->info(utl::BY,
  //               901,
  //               "min_len_isSame_{} {} {} {}",
  //               isSameNet,
  //               viaXName,
  //               viaYName,
  //               minLen);

  return minLen;
}

void Axioms::addAxiomsTriples(
    odb::dbTechLayer* techLayer,
    std::vector<std::pair<std::string, odb::dbTechVia*>> defaultViasInLayer,
    CalcLength calcFunc,
    bool isSameNet,
    bool excludedNullVia)
{
  if (!excludedNullVia) {
    axiomsTriples.push_back(
        {0, 0, (this->*calcFunc)(techLayer, nullptr, nullptr, isSameNet)});

    for (const auto& nameToViaX : defaultViasInLayer) {
      auto viaXIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaX.first);
      auto viaX = nameToViaX.second;
      axiomsTriples.push_back(
          {viaXIdx, 0, (this->*calcFunc)(techLayer, viaX, nullptr, isSameNet)});
    }
    for (const auto& nameToViaY : defaultViasInLayer) {
      auto viaYIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaY.first);
      auto viaY = nameToViaY.second;
      axiomsTriples.push_back(
          {0, viaYIdx, (this->*calcFunc)(techLayer, nullptr, viaY, isSameNet)});
    }
  }

  for (const auto& nameToViaX : defaultViasInLayer) {
    auto viaXIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaX.first);
    auto viaX = nameToViaX.second;
    for (const auto& nameToViaY : defaultViasInLayer) {
      auto viaYIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaY.first);
      auto viaY = nameToViaY.second;
      axiomsTriples.push_back(
          {viaXIdx,
           viaYIdx,
           (this->*calcFunc)(techLayer, viaX, viaY, isSameNet)});
    }
  }
}

uint64_t Axioms::calcTrunkDistLength(odb::dbTechLayer* techLayer,
                                     odb::dbTechVia* viaX,
                                     odb::dbTechVia* viaY,
                                     bool isSameNet)
{
  auto layerDir = techLayer->getDirection();

  uint64_t length = 0;

  auto metalBoxOfViaX = getLayerBoxFromVia(techLayer, viaX);
  auto metalBoxOfViaY = getLayerBoxFromVia(techLayer, viaY);

  odb::Rect* boundaryBoxOfViaX = new odb::Rect();
  odb::Rect* boundaryBoxOfViaY = new odb::Rect();
  getBoundaryBoxOfCutBoxesFromVia(viaX, boundaryBoxOfViaX);
  getBoundaryBoxOfCutBoxesFromVia(viaY, boundaryBoxOfViaY);

  int cutSpacing = 0;
  if (viaX != nullptr && viaY != nullptr) {
    cutSpacing = getCutSpacingFromVia(viaX, viaY, isSameNet);
    if (layerDir == odb::dbTechLayerDir::HORIZONTAL) {
      cutSpacing -= dbAdapter_.defdist(
          abs(metalBoxOfViaX->xMax() - boundaryBoxOfViaX->xMax()));
      cutSpacing -= dbAdapter_.defdist(
          abs(metalBoxOfViaY->xMin() - boundaryBoxOfViaY->xMin()));
    } else {
      cutSpacing -= dbAdapter_.defdist(
          abs(metalBoxOfViaX->yMax() - boundaryBoxOfViaX->yMax()));
      cutSpacing -= dbAdapter_.defdist(
          abs(metalBoxOfViaY->yMin() - boundaryBoxOfViaY->yMin()));
    }
  }

  auto routingSpacing = dbAdapter_.defdist(getRoutingEOLSpacing(techLayer, isSameNet));
  length = std::max(cutSpacing, routingSpacing);

  // std::string viaXName = viaX == nullptr ? "null" : viaX->getName();
  // std::string viaYName = viaY == nullptr ? "null" : viaY->getName();

  // logger_->info(utl::BY,
  //               901,
  //               "dist_len_isSame_{} {} {} {} {} {}",
  //               isSameNet,
  //               viaXName,
  //               viaYName,
  //               cutSpacing,
  //               routingSpacing,
  //               length);

  delete boundaryBoxOfViaX;
  delete boundaryBoxOfViaY;

  return (uint64_t) length;
}

uint64_t Axioms::calcViaTrunkLength(odb::dbTechLayer* techLayer,
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

  uint64_t borderBoxOfViaXToEdge = 0;
  uint64_t borderBoxOfViaYToEdge = 0;

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
      borderBoxOfViaXToEdge = dbAdapter_.defdist(abs(boundaryBoxOfViaX->xMax() - metalBoxOfViaX->xMin()));
      borderCutBoxOfViaXToMetalBoxEdge = getMarginBetweenCutAndMetalBox(metalBoxOfViaX, boundaryBoxOfViaX, POSITION::RIGHT);

      if (dbAdapter_.defdist(metalBoxOfViaX->getDY()) > width) {
        diffLenOfBoxOfViaX
            = dbAdapter_.defdist(metalBoxOfViaX->getDY()) - width;
      }
    }

    if (viaY != nullptr) {
      borderBoxOfViaYToEdge = dbAdapter_.defdist(
          abs(boundaryBoxOfViaY->xMin() - metalBoxOfViaY->xMax()));
      borderCutBoxOfViaYToMetalBoxEdge = getMarginBetweenCutAndMetalBox(metalBoxOfViaX, boundaryBoxOfViaX, POSITION::LEFT);
      if (dbAdapter_.defdist(metalBoxOfViaY->getDY()) > width) {
        diffLenOfBoxOfViaY
            = dbAdapter_.defdist(metalBoxOfViaY->getDY()) - width;
      }
    }

  } else {
    if (viaX != nullptr) {
      borderBoxOfViaXToEdge = dbAdapter_.defdist(abs(boundaryBoxOfViaX->yMax() - metalBoxOfViaX->yMin()));
      borderCutBoxOfViaXToMetalBoxEdge = getMarginBetweenCutAndMetalBox(metalBoxOfViaX, boundaryBoxOfViaX, POSITION::TOP);
      if (dbAdapter_.defdist(metalBoxOfViaX->getDX()) > width) {
        diffLenOfBoxOfViaX
            = dbAdapter_.defdist(metalBoxOfViaX->getDX()) - width;
      }
    }

    if (viaY != nullptr) {
      borderBoxOfViaYToEdge = dbAdapter_.defdist(abs(boundaryBoxOfViaY->yMin() - metalBoxOfViaY->yMax()));
      borderCutBoxOfViaYToMetalBoxEdge = getMarginBetweenCutAndMetalBox(metalBoxOfViaX, boundaryBoxOfViaX, POSITION::BOTTOM);
      if (dbAdapter_.defdist(metalBoxOfViaY->getDX()) > width) {
        diffLenOfBoxOfViaY
            = dbAdapter_.defdist(metalBoxOfViaY->getDX()) - width;
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
      && (cutSpacing - borderCutBoxOfViaXToMetalBoxEdge - borderCutBoxOfViaYToMetalBoxEdge) < minStep) {
    cutSpacing = minStep + borderCutBoxOfViaXToMetalBoxEdge + borderCutBoxOfViaYToMetalBoxEdge;
  }

  minLen = borderBoxOfViaXToEdge + borderBoxOfViaYToEdge + cutSpacing;

  
  delete boundaryBoxOfViaX;
  delete boundaryBoxOfViaY;

  return minLen;
}

uint64_t Axioms::calcViaTrackLength(odb::dbTechLayer* techLayer,
                                    odb::dbTechVia* via)
{
  auto layerDir = techLayer->getDirection();
  uint64_t spacing = getCutSpacingFromVia(via, via, false);
  auto metalBoxOfVia = getLayerBoxFromVia(techLayer, via);
  odb::Rect* boundaryBoxOfVia = new odb::Rect();
  getBoundaryBoxOfCutBoxesFromVia(via, boundaryBoxOfVia);

  if (layerDir == odb::dbTechLayerDir::HORIZONTAL) {
     spacing -= getMarginBetweenCutAndMetalBox(metalBoxOfVia, boundaryBoxOfVia, POSITION::TOP);
  } else {
     spacing -= getMarginBetweenCutAndMetalBox(metalBoxOfVia, boundaryBoxOfVia, POSITION::RIGHT);
  }

  delete boundaryBoxOfVia;

  return std::max(spacing, getRoutingPRLSpacing(techLayer, false));
}

uint64_t Axioms::calcViaTrackLength(odb::dbTechLayer* techLayer,
                                    odb::dbTechVia* viaX,
                                    odb::dbTechVia* viaY,
                                    bool isSameNet)
{
  if (techLayer == nullptr || viaX == nullptr || viaY == nullptr) {
    return 0;
  }
  auto layerDir = techLayer->getDirection();

  auto metalBoxOfViaX = getLayerBoxFromVia(techLayer, viaX);
  auto metalBoxOfViaY = getLayerBoxFromVia(techLayer, viaY);

  odb::Rect* boundaryBoxOfViaX = new odb::Rect();
  odb::Rect* boundaryBoxOfViaY = new odb::Rect();

  getBoundaryBoxOfCutBoxesFromVia(viaX, boundaryBoxOfViaX);
  getBoundaryBoxOfCutBoxesFromVia(viaY, boundaryBoxOfViaY);
  
  int cutSpacing = getCutSpacingFromVia(viaX, viaY, isSameNet);
  if (layerDir == odb::dbTechLayerDir::HORIZONTAL) {
    cutSpacing -= getMarginBetweenCutAndMetalBox(
        metalBoxOfViaX, boundaryBoxOfViaX, POSITION::TOP);
    cutSpacing -= getMarginBetweenCutAndMetalBox(
        metalBoxOfViaY, boundaryBoxOfViaY, POSITION::BOTTOM);
  } else {
    cutSpacing -= getMarginBetweenCutAndMetalBox(
        metalBoxOfViaX, boundaryBoxOfViaX, POSITION::RIGHT);
    cutSpacing -= getMarginBetweenCutAndMetalBox(
        metalBoxOfViaY, boundaryBoxOfViaY, POSITION::LEFT);
  }
  uint64_t routingSpacing = getRoutingPRLSpacing(techLayer, isSameNet);
  uint64_t spacing = routingSpacing > cutSpacing ? routingSpacing : cutSpacing ;

  delete boundaryBoxOfViaX;
  delete boundaryBoxOfViaY;

  return spacing;
}

void Axioms::initAxiomsTables()
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
    uint64_t triplesBegin = axiomsTriples.size();
    addAxiomsTriples(
        techLayer, defaultViasInLayer, &Axioms::calcTrunkMinLength, false);
    layerToTrunkMinLengthTable.push_back(
        {layerIdx, triplesBegin, axiomsTriples.size()});
    //===============================================================================================
    triplesBegin = axiomsTriples.size();
    addAxiomsTriples(
        techLayer, defaultViasInLayer, &Axioms::calcTrunkDistLength, true);
    layerToTrunkDistSameTable.push_back(
        {layerIdx, triplesBegin, axiomsTriples.size()});
    //===============================================================================================
    triplesBegin = axiomsTriples.size();
    addAxiomsTriples(
        techLayer, defaultViasInLayer, &Axioms::calcTrunkDistLength, false);
    layerToTrunkDistDiffTable.push_back(
        {layerIdx, triplesBegin, axiomsTriples.size()});
    //===============================================================================================
    triplesBegin = axiomsTriples.size();
    addAxiomsTriples(
        techLayer, defaultViasInLayer, &Axioms::calcViaTrunkLength, false);
    layerToTrunkViaTable.push_back(
        {layerIdx, triplesBegin, axiomsTriples.size()});
    //===============================================================================================
    triplesBegin = axiomsTriples.size();
    addAxiomsTriples(
        techLayer, defaultViasInLayer, &Axioms::calcViaTrackLength, false, true);
    layerToViaTrackDoubleViaSameTable.push_back(
        {layerIdx, triplesBegin, axiomsTriples.size()});
    //===============================================================================================
    triplesBegin = axiomsTriples.size();
    addAxiomsTriples(
        techLayer, defaultViasInLayer, &Axioms::calcViaTrackLength, false, true);
    layerToViaTrackDoubleViaDiffTable.push_back(
        {layerIdx, triplesBegin, axiomsTriples.size()});    
    //===============================================================================================
    triplesBegin = axiomsTriples.size();
    for (auto viaIter : defaultViasInLayer) {
      auto viaIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, viaIter.first);
      auto via = viaIter.second;
      axiomsTriples.push_back({viaIdx, 0, calcViaTrackLength(techLayer, via)});
    }
    layerToViaTrackTable.push_back({layerIdx, triplesBegin, axiomsTriples.size()});
  }
}

}  // namespace banyan