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

odb::dbBox* Axioms::getCutBoxInBorderFromVia(odb::dbTechVia* via,
                                             POSITION position)
{
  if (via == nullptr) {
    return nullptr;
  }
  odb::dbBox* resultBox = nullptr;
  for (auto box : via->getBoxes()) {
    if (box->getTechLayer()->getType().getValue()
        != odb::dbTechLayerType::CUT) {
      continue;
    }
    if (resultBox == nullptr) {
      resultBox = box;
    } else {
      switch (position) {
        case POSITION::LEFT:
          if (box->xMin() < resultBox->xMin()) {
            resultBox = box;
          }
          break;
        case POSITION::BOTTOM:
          if (box->yMin() < resultBox->yMin()) {
            resultBox = box;
          }
          break;
        case POSITION::RIGHT:
          if (box->xMax() > resultBox->xMax()) {
            resultBox = box;
          }
          break;
        case POSITION::TOP:
          if (box->yMax() > resultBox->yMax()) {
            resultBox = box;
          }
          break;
      }
    }
  }
  return resultBox;
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
    if (box->getTechLayer()->getType().getValue() == odb::dbTechLayerType::CUT) {
      cutLayer = box->getTechLayer();
      break;
    }
  }
  return cutLayer;
}

uint64_t Axioms::getCutSpacingForTrunkMinLengthFromVia(odb::dbTechVia* viaX,
                                                       odb::dbTechVia* viaY)
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

  odb::dbSet<odb::dbTechLayerSpacingRule> spcingRules;
  cutLayerForViaX->getV54SpacingRules(spcingRules);

  for (auto cutSpacingRule : spcingRules) {
    if (cutSpacingRule->getCutSameNet()) {
      if (cutSpacingRule->getSpacing() > cutSpacing) {
        cutSpacing = cutSpacingRule->getSpacing();
      }
    }
  }

  if (cutSpacing <= 0) {
    cutSpacing = cutLayerForViaX->getSpacing();
  }

  // for (auto cutSpacingRule : cutLayerForViaX->getTechLayerCutSpacingRules()) {
  //   if (cutSpacingRule->isExceptSameNet() || cutSpacingRule->isExceptSameMetal()
  //       || cutSpacingRule->isSameVia()) {
  //     continue;
  //   }
  //   if (cutSpacingRule->getCutSpacing() > cutSpacing) {
  //     cutSpacing = cutSpacingRule->getCutSpacing();
  //   }
  // }

  // for (auto cutSpacingRule : cutLayerForViaY->getTechLayerCutSpacingRules()) {
  //   if (cutSpacingRule->isExceptSameNet() || cutSpacingRule->isExceptSameMetal()
  //       || cutSpacingRule->isSameVia()) {
  //     continue;
  //   }
  //   if (cutSpacingRule->getCutSpacing() > cutSpacing) {
  //     cutSpacing = cutSpacingRule->getCutSpacing();
  //   }
  // }

  return (uint64_t) cutSpacing;
}

uint64_t Axioms::calcTrunkMinLength(odb::dbTechLayer* techLayer,
                                    odb::dbTechVia* viaX,
                                    odb::dbTechVia* viaY)
{
  uint64_t minLen = 0;
  auto width = techLayer->getWidth();
  auto layerDir = techLayer->getDirection();
  auto area = dbAdapter_.defArea(techLayer->getArea());
  uint64_t cutSpacing = getCutSpacingForTrunkMinLengthFromVia(viaX, viaY);

  auto metalBoxOfViaX = getLayerBoxFromVia(techLayer, viaX);
  auto metalBoxOfViaY = getLayerBoxFromVia(techLayer, viaY);

  odb::dbBox* borderCutBoxOfViaX;
  odb::dbBox* borderCutBoxOfViaY;

  uint64_t borderBoxOfViaXToEdge = 0;
  uint64_t borderBoxOfViaYToEdge = 0;

  uint64_t diffAreaOfBoxOfViaX = 0;
  uint64_t diffAreaOfBoxOfViaY = 0;

  // uint32_t cutNumsOfViaX = getCutNumsFromVia(viaX);
  // uint32_t cutNumsOfViaY = getCutNumsFromVia(viaY);

  uint32_t borderCutBoxOfViaXToMetalBoxEdge = 0;
  uint32_t borderCutBoxOfViaYToMetalBoxEdge = 0;

  if (layerDir == odb::dbTechLayerDir::HORIZONTAL) {
    borderCutBoxOfViaX = getCutBoxInBorderFromVia(viaX, POSITION::RIGHT);
    if (metalBoxOfViaX != nullptr && borderCutBoxOfViaX != nullptr) {
      borderBoxOfViaXToEdge = dbAdapter_.defdist(
          abs(borderCutBoxOfViaX->xMax() - metalBoxOfViaX->xMin()));
      borderCutBoxOfViaXToMetalBoxEdge = dbAdapter_.defdist(
          abs(metalBoxOfViaX->xMax() - borderCutBoxOfViaX->xMax()));
      if (dbAdapter_.defdist(metalBoxOfViaX->getDY()) > width) {
        diffAreaOfBoxOfViaX = (dbAdapter_.defdist(metalBoxOfViaX->getDY()) - width)
                              * dbAdapter_.defdist(metalBoxOfViaX->getDX());
      }
    }

    borderCutBoxOfViaY = getCutBoxInBorderFromVia(viaY, POSITION::LEFT);
    if (metalBoxOfViaY != nullptr && borderCutBoxOfViaY != nullptr) {
      borderBoxOfViaYToEdge = dbAdapter_.defdist(
          abs(borderCutBoxOfViaY->xMin() - metalBoxOfViaY->xMax()));
      borderCutBoxOfViaYToMetalBoxEdge = dbAdapter_.defdist(
          abs(metalBoxOfViaY->xMin() - borderCutBoxOfViaY->xMin()));
      if (dbAdapter_.defdist(metalBoxOfViaY->getDY()) > width) {
        diffAreaOfBoxOfViaY = (dbAdapter_.defdist(metalBoxOfViaY->getDY()) - width)
                              * dbAdapter_.defdist(metalBoxOfViaY->getDX());
      }
    }

  } else {
    borderCutBoxOfViaX = getCutBoxInBorderFromVia(viaX, POSITION::TOP);
    if (metalBoxOfViaX != nullptr && borderCutBoxOfViaX != nullptr) {
      borderBoxOfViaXToEdge = dbAdapter_.defdist(
          abs(borderCutBoxOfViaX->yMax() - metalBoxOfViaX->yMin()));
      borderCutBoxOfViaXToMetalBoxEdge = dbAdapter_.defdist(
          abs(metalBoxOfViaX->yMax() - borderCutBoxOfViaX->yMax()));
      if (dbAdapter_.defdist(metalBoxOfViaX->getDX()) > width) {
        diffAreaOfBoxOfViaX = (dbAdapter_.defdist(metalBoxOfViaX->getDX()) - width)
                              * dbAdapter_.defdist(metalBoxOfViaX->getDY());
      }
    }

    borderCutBoxOfViaY = getCutBoxInBorderFromVia(viaY, POSITION::BOTTOM);
    if (metalBoxOfViaY != nullptr && borderCutBoxOfViaY != nullptr) {
      borderBoxOfViaYToEdge = dbAdapter_.defdist(
          abs(borderCutBoxOfViaY->yMin() - metalBoxOfViaY->yMax()));
      borderCutBoxOfViaYToMetalBoxEdge = dbAdapter_.defdist(
          abs(metalBoxOfViaY->yMin() - borderCutBoxOfViaY->yMin()));
      if (dbAdapter_.defdist(metalBoxOfViaY->getDX()) > width) {
        diffAreaOfBoxOfViaY = (dbAdapter_.defdist(metalBoxOfViaY->getDX()) - width)
                              * dbAdapter_.defdist(metalBoxOfViaY->getDY());
      }
    }
  }

  auto minStep = techLayer->getMinStep();
  if (minStep > 0 && diffAreaOfBoxOfViaX > 0 && diffAreaOfBoxOfViaY > 0
      && (cutSpacing - borderCutBoxOfViaXToMetalBoxEdge - borderCutBoxOfViaYToMetalBoxEdge) < minStep) {
    cutSpacing += minStep + borderCutBoxOfViaXToMetalBoxEdge + borderCutBoxOfViaYToMetalBoxEdge - cutSpacing;
  }

  minLen = borderBoxOfViaXToEdge + borderBoxOfViaYToEdge + cutSpacing;
  int64_t remainArea = area - diffAreaOfBoxOfViaX - diffAreaOfBoxOfViaY;

  if (remainArea > 0 && minLen * width < remainArea) {
    uint64_t extLen = (remainArea - minLen * width) / width;
    minLen += extLen;
  }

  return minLen;
}

void Axioms::initLayerToTrunkMinLengthTable()
{
  odb::dbTech* tech = db_->getTech();
  for (auto techLayer : tech->getLayers()) {
    if (techLayer->getType().getValue() != odb::dbTechLayerType::ROUTING) {
      continue;
    }
    int layerIdx
        = AssertFind(dbAdapter_.layerNameToIdxs_, techLayer->getName());
    std::vector<std::pair<std::string, odb::dbTechVia*>> defaultViasInLayer;

    copy_if(defaultViaMap.begin(),
            defaultViaMap.end(),
            std::inserter(defaultViasInLayer, defaultViasInLayer.end()),
            [techLayer](auto const& kv_pair) {
              auto via = kv_pair.second;
              return via->getTopLayer()->getName() == techLayer->getName()
                     || via->getBottomLayer()->getName()
                            == techLayer->getName();
            });


    uint64_t triplesBegin = axiomsTriples.size();

    axiomsTriples.push_back(
        {0, 0, calcTrunkMinLength(techLayer, nullptr, nullptr)});
    for (const auto& nameToViaX : defaultViasInLayer) {
      auto viaXIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaX.first);
      auto viaX = nameToViaX.second;
      axiomsTriples.push_back(
          {viaXIdx, 0, calcTrunkMinLength(techLayer, viaX, nullptr)});
    }
    for (const auto& nameToViaY : defaultViasInLayer) {
      auto viaYIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaY.first);
      auto viaY = nameToViaY.second;
      axiomsTriples.push_back(
          {0, viaYIdx, calcTrunkMinLength(techLayer, nullptr, viaY)});
    }
    for (const auto& nameToViaX : defaultViasInLayer) {
      auto viaXIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaX.first);
      auto viaX = nameToViaX.second;
      for (const auto& nameToViaY : defaultViasInLayer) {
        auto viaYIdx
            = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaY.first);
        auto viaY = nameToViaY.second;
        axiomsTriples.push_back(
            {viaXIdx, viaYIdx, calcTrunkMinLength(techLayer, viaX, viaY)});
      }
    }
    layerToTrunkMinLengthTable.push_back(
        {layerIdx, triplesBegin, axiomsTriples.size()});
  }
}

void Axioms::initLayerToTrunkDistSameTable()
{
}

void Axioms::initLayerToTrunkDistDiffTable()
{
}

}  // namespace banyan