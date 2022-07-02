#include "axioms/Axioms.hpp"

#include <assert.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <utility>
#include <utils.hpp>

namespace banyan {


void Axioms::initViaMap()
{
  for (auto techVia : db_->getTech()->getVias()) {
    if (!techVia->isDefault()) {
      continue;
    }
    viaMap[techVia->getName()] = std::make_shared<Via>(Via(techVia));
  }

#ifndef NDEBUG
  printer.print("Vias:");
  for (auto [_, viaPtr] : viaMap) {
    printer.print(*viaPtr);
  }
#endif
}

void Axioms::initCutClasses()
{
  odb::dbTech* tech = db_->getTech();

  for (auto layer : tech->getLayers()) {
    if (layer->getType() != odb::dbTechLayerType::CUT) {
      continue;
    }
    CutClassMap<CutClassRule> cutClassRules;
    for (auto cutClassRule : layer->getTechLayerCutClassRules()) {
      cutClassRules[cutClassRule->getName()]
          = std::make_shared<CutClassRule>(CutClassRule(layer, cutClassRule));
    }
    CutClassRule::laToCutClassRules[layer] = std::move(cutClassRules);
  }
}

void Axioms::initRules()
{
  odb::dbTech* tech = db_->getTech();

  std::vector<uint> widthIdx;
  std::vector<uint> lengthIdx;
  for (auto layer : tech->getLayers()) {
    LayerRules<CutSpacingRule> cutSpacingRules;
    LayerRules<CutSpacingTableRule> cutSpacingTableRules;
    LayerRules<RoutingSpacingRule> routingSpacingRules;
    LayerRules<RoutingSpacingTableRule> routingSpacingTableRules;
    LayerRules<RoutingEOLSpacingRule> routingEOLSpacingRules;
    odb::dbSet<odb::dbTechLayerSpacingRule> spacingRules;

    switch (layer->getType()) {
      case odb::dbTechLayerType::CUT:
        if (layer->getV54SpacingRules(spacingRules)) {
          for (auto spacingRule : spacingRules) {
            cutSpacingRules.push_back(std::make_shared<CutSpacingRule>(
                CutSpacingRule(layer, spacingRule)));
          }
        }
        for (auto cutSpacingRule : layer->getTechLayerCutSpacingRules()) {
          cutSpacingRules.push_back(std::make_shared<CutSpacingRule>(
              CutSpacingRule(layer, cutSpacingRule)));
        }
        for (auto cutSpacingTblRule :
             layer->getTechLayerCutSpacingTableDefRules()) {
          cutSpacingTableRules.push_back(std::make_shared<CutSpacingTableRule>(
              CutSpacingTableRule(layer, cutSpacingTblRule)));
        }
        laToCutSpacingRules[layer] = std::move(cutSpacingRules);
        laToCutSpacingTableRules[layer] = std::move(cutSpacingTableRules);
        break;
      case odb::dbTechLayerType::ROUTING:
        printer.print(layer->getName(),
                      "\tMinArea ",
                      dbAdapter_.defArea(layer->getArea()));
        if (layer->getV54SpacingRules(spacingRules)) {
          for (auto spacingRule : spacingRules) {
            if (spacingRule->hasSpacingNotchLength()) {
              printer.print(
                  layer->getName(),
                  "\taxioms skip routing spacing rule with notchlength");
              continue;
            }
            routingSpacingRules.push_back(std::make_shared<RoutingSpacingRule>(
                RoutingSpacingRule(layer, spacingRule)));
          }
        }
        for (auto spacingRule : layer->getTechLayerSpacingNotchLengthRules()) {
          printer.print(layer->getName(),
                        "\taxioms skip routing spacing rule with notchlength");
        }
        if (layer->hasV55SpacingRules()
            && layer->getV55SpacingWidthsAndLengths(widthIdx, lengthIdx)) {
          routingSpacingTableRules.push_back(
              std::make_shared<RoutingSpacingTableRule>(
                  RoutingSpacingTableRule(layer, widthIdx, lengthIdx)));
        }
        for (auto spacingTblPrl : layer->getTechLayerSpacingTablePrlRules()) {
          routingSpacingTableRules.push_back(
              std::make_shared<RoutingSpacingTableRule>(
                  RoutingSpacingTableRule(layer, spacingTblPrl)));
        }
        for (auto eolSpacingRule : layer->getTechLayerSpacingEolRules()) {
          routingEOLSpacingRules.push_back(
              std::make_shared<RoutingEOLSpacingRule>(
                  RoutingEOLSpacingRule(layer, eolSpacingRule)));
        }
        laToRoutingSpacingRules[layer] = std::move(routingSpacingRules);
        laToRoutingSpacingTableRules[layer]
            = std::move(routingSpacingTableRules);
        laToRoutingEOLSpacingRules[layer] = std::move(routingEOLSpacingRules);
        break;
    }
  }

 if (debugActive_) {
    printer.print("\nCUT Class Rules:");
    for (auto [_, rules] : CutClassRule::laToCutClassRules) {
      for (auto [_, rule] : rules) {
        printer.print(*rule);
      }
    }
    printer.print("\nCUT SPACING Rules:");
    for (auto [_, rules] : laToCutSpacingRules) {
      for (auto rule : rules) {
        printer.print(*rule);
      }
    }
    printer.print("\nCUT SPACINGTABLE Rules:");
    for (auto [_, rules] : laToCutSpacingTableRules) {
      for (auto rule : rules) {
        printer.print(*rule);
      }
    }
    printer.print("\nSPACING Rules:");
    for (auto [_, rules] : laToRoutingSpacingRules) {
      for (auto rule : rules) {
        printer.print(*rule);
      }
    }
    printer.print("\nSPACINGTABLE Rules:");
    for (auto [_, rules] : laToRoutingSpacingTableRules) {
      for (auto rule : rules) {
        printer.print(*rule);
      }
    }
    printer.print("\nEOL SPACING Rules:");
    for (auto [_, rules] : laToRoutingEOLSpacingRules) {
      for (auto rule : rules) {
        printer.print(*rule);
      }
    }
  }
}

void Axioms::getViasInLayer(
    odb::dbTechLayer* layer,
    std::vector<std::pair<std::string, std::shared_ptr<Via>>>& viasInLayer)
{
  std::copy_if(viaMap.begin(),
               viaMap.end(),
               std::inserter(viasInLayer, viasInLayer.end()),
               [layer](auto const& kv_pair) {
                 auto via = kv_pair.second;
                 return via->topLayer()->getName() == layer->getName()
                        || via->bottomLayer()->getName() == layer->getName();
               });
}

std::pair<uint64_t, uint64_t> Axioms::addTableTriples(
    odb::dbTechLayer* layer,
    std::vector<std::pair<std::string, std::shared_ptr<Via>>> viasInLayer,
    CalcLengthFn calcFunc,
    bool isSameNet,
    std::function<bool(int, int)> viaPairFilter)
{
  std::vector<banyan_go::table_triple>& triples = dbAdapter_.tableTriples_;
  uint64_t triplesBegin = triples.size();

  if (viaPairFilter(NULL_VIA_IDX, NULL_VIA_IDX)) {
    triples.push_back({NULL_VIA_IDX,
                       NULL_VIA_IDX,
                       (this->*calcFunc)(layer, nullptr, nullptr, isSameNet)});
  }

  for (const auto& [viaXName, viaX] : viasInLayer) {
    auto viaXIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, viaXName);
    if (viaPairFilter(viaXIdx, NULL_VIA_IDX)) {
      triples.push_back({viaXIdx,
                         NULL_VIA_IDX,
                         (this->*calcFunc)(layer, viaX, nullptr, isSameNet)});
    }
  }

  for (const auto& nameToViaY : viasInLayer) {
    auto viaYIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaY.first);
    auto viaY = nameToViaY.second;
    if (viaPairFilter(NULL_VIA_IDX, viaYIdx)) {
      triples.push_back({NULL_VIA_IDX,
                         viaYIdx,
                         (this->*calcFunc)(layer, nullptr, viaY, isSameNet)});
    }
  }

  for (const auto& nameToViaX : viasInLayer) {
    auto viaXIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaX.first);
    auto viaX = nameToViaX.second;
    for (const auto& nameToViaY : viasInLayer) {
      auto viaYIdx = AssertFind(dbAdapter_.viaDefNameToIdxs_, nameToViaY.first);
      auto viaY = nameToViaY.second;
      if (viaPairFilter(viaXIdx, viaYIdx)) {
        triples.push_back({viaXIdx,
                           viaYIdx,
                           (this->*calcFunc)(layer, viaX, viaY, isSameNet)});
      }
    }
  }

  uint64_t triplesEnd = triples.size();
  return std::make_pair(triplesBegin, triplesEnd);
}

uint64_t Axioms::getMinSpacing(odb::dbTechLayer* layer)
{
  uint64_t minSpacing = layer->getSpacing();
  auto rules = laToRoutingSpacingRules[layer];
  for (auto rule : rules) {
    minSpacing = minSpacing == 0
                     ? rule->getSpacing()
                     : std::min(minSpacing, (uint64_t) rule->getSpacing());
  }

  auto ruleTbls = laToRoutingSpacingTableRules[layer];
  assert(ruleTbls.size() <= 1);

  return std::max(minSpacing,
                  ruleTbls.size() > 0 ? ruleTbls[0]->getMinSpacing() : 0);
}

uint64_t Axioms::getMaxRoutingPrlSpacing(odb::dbTechLayer* layer,
                                         bool isSameNet)
{
  uint64_t width = layer->getWidth();
  uint64_t sameNetSpacing = 0;
  uint64_t spacing = getMinSpacing(layer);

  for (auto rule : laToRoutingSpacingRules[layer]) {
    auto subRule = rule->getSubRule();
    if (subRule.has_value()) {
      std::visit(overloaded{
                     [width, rule, &spacing](RoutingSpacingRange& range) {
                       if (range.getMinWidth() <= width
                           && width <= range.getMaxWidth()) {
                         spacing
                             = std::max(spacing, (uint64_t) rule->getSpacing());
                       }
                     },
                     [rule, &sameNetSpacing](RoutingSpacingSameNet& sameNet) {
                       if (!sameNet.getIsPGOnly()) {
                         sameNetSpacing = std::max(
                             sameNetSpacing, (uint64_t) rule->getSpacing());
                       }
                     },
                     [rule](RoutingSpacingNotchLength&) {

                     },
                     [rule](RoutingSpacingLengthThreshold&) {

                     },
                 },
                 subRule.value());
    } else {
      spacing = std::max(spacing, (uint64_t) rule->getSpacing());
    }
  }

  auto ruleTbls = laToRoutingSpacingTableRules[layer];
  assert(ruleTbls.size() <= 1);
  spacing = std::max(spacing,
                     ruleTbls.size() > 0 ? ruleTbls[0]->getSpacing(
                         width, std::numeric_limits<uint32_t>::max())
                                         : spacing);

  return isSameNet ? std::max(spacing, sameNetSpacing) : spacing;
}

uint64_t Axioms::getMaxRoutingEOLPrlSpacing(odb::dbTechLayer* layer,
                                            std::shared_ptr<Via> via,
                                            bool isSameNet)
{
  uint64_t spacing = getMinSpacing(layer);

  auto rules = laToRoutingEOLSpacingRules[layer];
  for (auto rule : rules) {
    auto subRule = rule->getSubRule();
    std::visit(overloaded{
                   [](std::monostate&) {

                   },
                   [rule, &spacing](RoutingEOLSpacingRuleWithin& within) {
                     if (within.getPrl().has_value()) {
                     } else if (within.getEncloseCut().has_value()) {
                     } else {
                       spacing
                           = std::max(spacing, (uint64_t) rule->getEolSpace());
                     }
                   },
                   [rule](RoutingEOLSpacingRuleToConcaveCorner& subRule) {

                   },
                   [rule](RoutingEOLSpacingRuleToNotchLength& subRule) {

                   },
               },
               subRule);
  }

  return spacing;
}

uint64_t Axioms::getMaxRoutingEOLEndToEndSpacing(odb::dbTechLayer* layer,
                                                 std::shared_ptr<Via> via,
                                                 bool isSameNet)
{
  uint64_t spacing = getMinSpacing(layer);
  std::optional<uint64_t> eteSpacing;

  auto rules = laToRoutingEOLSpacingRules[layer];
  for (auto rule : rules) {
    auto subRule = rule->getSubRule();
    std::visit(
        overloaded{
            [](std::monostate&) {

            },
            [rule, &spacing, &eteSpacing](RoutingEOLSpacingRuleWithin& within) {
              if (within.getPrl().has_value()) {
              } else if (within.getEncloseCut().has_value()) {
              } else if (within.getEndToEnd().has_value()) {
                auto eteSubRule = within.getEndToEnd().value();
                if (eteSpacing.has_value()) {
                  eteSpacing = std::make_optional(
                      std::max(eteSpacing.value(),
                               (uint64_t) eteSubRule.getEndToEndSpace()));
                } else {
                  eteSpacing = std::make_optional(
                      (uint64_t) eteSubRule.getEndToEndSpace());
                }
              } else {
                spacing = std::max(spacing, (uint64_t) rule->getEolSpace());
              }
            },
            [rule](RoutingEOLSpacingRuleToConcaveCorner& subRule) {

            },
            [rule](RoutingEOLSpacingRuleToNotchLength& subRule) {

            },
        },
        subRule);
  }

  return eteSpacing.has_value() ? eteSpacing.value() : spacing;
}

std::vector<std::tuple<bool, std::optional<int64_t>, uint64_t>>
Axioms::getCutSpacings(std::shared_ptr<Via> via1,
                       std::shared_ptr<Via> via2,
                       bool isSameNet)
{
  std::vector<std::tuple<bool, std::optional<int64_t>, uint64_t>> results;
  auto getCutSpacingsFn = [this, &results, isSameNet](
                              std::shared_ptr<Via> mainVia,
                              std::shared_ptr<Via> subVia) {
    std::vector<std::tuple<bool, std::optional<int64_t>, uint64_t>> defSpacings;
    std::vector<std::tuple<bool, std::optional<int64_t>, uint64_t>>
        sameNetSpacings;
    for (auto rule : this->laToCutSpacingRules[mainVia->getViaLayer()]) {
      auto subRule = rule->getSubRule();
      if (subRule.has_value()) {
        std::visit(
            overloaded{
                [rule](CutSpacingSameMask& sameMask) {

                },
                [rule](CutSpacingMAXXY& maxxy) {

                },
                [&defSpacings,
                 &sameNetSpacings,
                 rule,
                 isSameNet,
                 &mainVia,
                 &subVia](CutSpacing& spacing) {
                  if (spacing.conditionValid(isSameNet,
                                             mainVia->getViaLayer(),
                                             subVia->getViaLayer())) {
                    if (spacing.isSameNetRule()) {
                      sameNetSpacings.push_back({spacing.getCenterToCenter(),
                                                 std::nullopt,
                                                 rule->getSpacing()});
                    } else {
                      defSpacings.push_back({spacing.getCenterToCenter(),
                                             std::nullopt,
                                             rule->getSpacing()});
                    }
                  }
                },
            },
            subRule.value());

      } else {
        defSpacings.push_back({false, std::nullopt, rule->getSpacing()});
      }
    }

    for (auto rule : laToCutSpacingTableRules[mainVia->getViaLayer()]) {
      auto spacings = rule->getCutSpacings(mainVia, subVia, false);
      defSpacings.insert(defSpacings.end(), spacings.begin(), spacings.end());
      if (isSameNet) {
        spacings = rule->getCutSpacings(mainVia, subVia, true);
        sameNetSpacings.insert(
            sameNetSpacings.end(), spacings.begin(), spacings.end());
      }
    }
    if (isSameNet && sameNetSpacings.size() > 0) {
      results.insert(
          results.end(), sameNetSpacings.begin(), sameNetSpacings.end());
    } else {
      results.insert(results.end(), defSpacings.begin(), defSpacings.end());
    }
  };

  getCutSpacingsFn(via1, via2);
  if (via1->getViaLayer() != via2->getViaLayer()) {
    getCutSpacingsFn(via2, via1);
  }
  return results;
}

uint64_t Axioms::getMaxCutPrlCenterToCenterSpacing(odb::dbTechLayer* layer,
                                                   std::shared_ptr<Via> viaX,
                                                   std::shared_ptr<Via> viaY,
                                                   bool isSameNet)
{
  auto isHorizontal = layer->getDirection() == odb::dbTechLayerDir::HORIZONTAL;

  uint64_t cutSpacing = 0;
  if (viaX != nullptr && viaY != nullptr) {
    auto cutsBboxOfViaX = viaX->getCutsBBox();
    auto cutsBboxOfViaY = viaY->getCutsBBox();

    decltype(cutSpacing) lineCutBBoxPadding
        = isHorizontal ? getBoxCenterToRightEdge(cutsBboxOfViaX)
                             + getBoxCenterToLeftEdge(cutsBboxOfViaY)
                       : getBoxCenterToTopEdge(cutsBboxOfViaX)
                             + getBoxCenterToBottomEdge(cutsBboxOfViaY);
    for (auto [isCenterToCenter, _, spacing] :
         getCutSpacings(viaX, viaY, isSameNet)) {
      cutSpacing
          = std::max(cutSpacing,
                     isCenterToCenter ? spacing : spacing + lineCutBBoxPadding);
    }
  }
  return cutSpacing;
}

std::optional<std::pair<Rectangle, Rectangle>> Axioms::getConstrainedMetalBoxes(
    odb::dbTechLayer* layer,
    std::shared_ptr<Via> via1,
    std::shared_ptr<Via> via2)
{
  auto via1MetalBox = via1->getMetalBox(layer);
  auto via2MetalBox = via2->getMetalBox(layer);

  return (via1MetalBox.has_value() && via2MetalBox.has_value())
             ? std::make_optional(
                 std::make_pair(via1MetalBox.value(), via2MetalBox.value()))
             : std::nullopt;
}

uint64_t Axioms::calcViaViaTrunkLength(odb::dbTechLayer* layer,
                                       std::shared_ptr<Via> viaX,
                                       std::shared_ptr<Via> viaY,
                                       bool isSameNet)
{
  assert(layer->getType() == odb::dbTechLayerType::ROUTING);
  assert(isSameNet);
  if (viaX == nullptr || viaY == nullptr) {
    return 0;
  }
  uint64_t width = layer->getWidth();
  auto isHorizontal = layer->getDirection() == odb::dbTechLayerDir::HORIZONTAL;

  auto ctcSpacing = getMaxCutPrlCenterToCenterSpacing(layer, viaX, viaY, true);

  if (DEBUG_VIAVIATRUNKLENGTH) {
    printer.print(layer->getName(),
                  " calcViaViaTrunkLength Via1 ",
                  viaX == nullptr ? "NullVia" : viaX->getViaName(),
                  " Via2 ",
                  viaY == nullptr ? "NullVia" : viaY->getViaName(),
                  " result ",
                  ctcSpacing);
  }
  return ctcSpacing;
}

uint64_t Axioms::calcTrunkMinLength(odb::dbTechLayer* layer,
                                    std::shared_ptr<Via> viaX,
                                    std::shared_ptr<Via> viaY,
                                    bool isSameNet)
{
  assert(layer->getType() == odb::dbTechLayerType::ROUTING);
  uint64_t width = layer->getWidth();
  auto isHorizontal = layer->getDirection() == odb::dbTechLayerDir::HORIZONTAL;

  decltype(width) minArea = dbAdapter_.defArea(layer->getArea());
  decltype(width) minLen = calcViaViaTrunkLength(layer, viaX, viaY, isSameNet);

  uint64_t diffAreaOfBoxOfViaX = 0;
  uint64_t diffAreaOfBoxOfViaY = 0;

  uint64_t metalBoxCenterToEdgeOfViaX = 0;
  uint64_t metalBoxCenterToEdgeOfViaY = 0;

  auto getDiffAreaBetweenViaAndWire = [](Rectangle metalBoxOfVia,
                                         uint64_t width,
                                         bool isHorizontal) -> int64_t {
    uint64_t detlaOfMetalBox
        = isHorizontal ? getBoxDx(metalBoxOfVia) : getBoxDy(metalBoxOfVia);
    uint64_t areaOfWire = detlaOfMetalBox * width;

    uint64_t areaOfMetalBox = area(metalBoxOfVia);

    return areaOfMetalBox < areaOfWire ? 0 : areaOfMetalBox - areaOfWire;
  };

  if (viaX != nullptr) {
    auto metalBoxOfViaX = viaX->getMetalBox(layer).value();
    diffAreaOfBoxOfViaX
        = getDiffAreaBetweenViaAndWire(metalBoxOfViaX, width, isHorizontal);
    metalBoxCenterToEdgeOfViaX = isHorizontal
                                     ? getBoxCenterToLeftEdge(metalBoxOfViaX)
                                     : getBoxCenterToBottomEdge(metalBoxOfViaX);
  }

  if (viaY != nullptr) {
    auto metalBoxOfViaY = viaY->getMetalBox(layer).value();
    diffAreaOfBoxOfViaY
        = getDiffAreaBetweenViaAndWire(metalBoxOfViaY, width, isHorizontal);
    metalBoxCenterToEdgeOfViaY = isHorizontal
                                     ? getBoxCenterToRightEdge(metalBoxOfViaY)
                                     : getBoxCenterToTopEdge(metalBoxOfViaY);
  }

  int64_t remainArea = minArea - diffAreaOfBoxOfViaX - diffAreaOfBoxOfViaY;
  uint64_t realLength = minLen + std::max(metalBoxCenterToEdgeOfViaX, width / 2)
                        + std::max(metalBoxCenterToEdgeOfViaY, width / 2);
  uint64_t realArea = realLength * width;

  if (remainArea > 0 && remainArea > (int64_t) realArea) {
    uint64_t extLen = (remainArea - realArea) / width;
    minLen += extLen;
  }
  if (DEBUG_TRUNKMINLENGTH) {
    printer.print(layer->getName(),
                  " calcTrunkMinLength Via1 ",
                  viaX == nullptr ? "NullVia" : viaX->getViaName(),
                  " Via2 ",
                  viaY == nullptr ? "NullVia" : viaY->getViaName(),
                  " Width ",
                  width,
                  " MinArea ",
                  minArea,
                  " Via1DiffArea",
                  diffAreaOfBoxOfViaX,
                  " Via2DiffArea",
                  diffAreaOfBoxOfViaY,
                  " Via1CenterToEdge ",
                  metalBoxCenterToEdgeOfViaX,
                  " Via2CenterToEdge ",
                  metalBoxCenterToEdgeOfViaY,
                  " result ",
                  minLen);
  }
  return minLen;
}

uint64_t Axioms::calcTrunkDistLength(odb::dbTechLayer* layer,
                                     std::shared_ptr<Via> viaX,
                                     std::shared_ptr<Via> viaY,
                                     bool isSameNet)
{
  assert(layer->getType() == odb::dbTechLayerType::ROUTING);
  auto isHorizontal = layer->getDirection() == odb::dbTechLayerDir::HORIZONTAL;
  uint64_t width = layer->getWidth();
  uint64_t pitch = isHorizontal ? layer->getPitchY() : layer->getPitchX();

  uint64_t metalBoxCenterToEdgeOfViaX = 0;
  if (viaX != nullptr) {
    auto mbOfViaX = viaX->getMetalBox(layer).value();
    metalBoxCenterToEdgeOfViaX = isHorizontal
                                     ? getBoxCenterToRightEdge(mbOfViaX)
                                     : getBoxCenterToTopEdge(mbOfViaX);
  }

  uint64_t metalBoxCenterToEdgeOfViaY = 0;
  if (viaY != nullptr) {
    auto mbOfViaY = viaY->getMetalBox(layer).value();
    metalBoxCenterToEdgeOfViaY = isHorizontal
                                     ? getBoxCenterToLeftEdge(mbOfViaY)
                                     : getBoxCenterToBottomEdge(mbOfViaY);
  }

  uint64_t cutCenterToCenterSpacing
      = getMaxCutPrlCenterToCenterSpacing(layer, viaX, viaY, isSameNet);

  uint64_t rtEOLEndToEndSpacing
      = std::max(getMaxRoutingEOLEndToEndSpacing(layer, viaX, isSameNet),
                 getMaxRoutingEOLEndToEndSpacing(layer, viaY, isSameNet));

  uint64_t spacing = std::max(
      rtEOLEndToEndSpacing + std::max(metalBoxCenterToEdgeOfViaX, width / 2)
          + std::max(metalBoxCenterToEdgeOfViaY, width / 2),
      cutCenterToCenterSpacing);

  if (DEBUG_TRUNKDISTLENGTH) {
    printer.print(layer->getName(),
                  " calcTrunkDistLength Via1 ",
                  viaX == nullptr ? "NullVia" : viaX->getViaName(),
                  " Via2 ",
                  viaY == nullptr ? "NullVia" : viaY->getViaName(),
                  " Width ",
                  width,
                  " CutCenterToCenterSpacing ",
                  cutCenterToCenterSpacing,
                  " EndToEndSpacing ",
                  rtEOLEndToEndSpacing,
                  " Via1CenterToEdge ",
                  metalBoxCenterToEdgeOfViaX,
                  " Via2CenterToEdge ",
                  metalBoxCenterToEdgeOfViaY,
                  " result ",
                  spacing);
  }
  return spacing;
}

uint64_t Axioms::calcViaViaTrackLength(odb::dbTechLayer* layer,
                                       std::shared_ptr<Via> viaX,
                                       std::shared_ptr<Via> viaY,
                                       bool isSameNet)
{
  assert(layer->getType() == odb::dbTechLayerType::ROUTING);
  assert(viaX != nullptr && viaY != nullptr);

  std::optional<uint64_t> maxPerfCTCSpacingWithBoxes;
  auto updateCTCSpacing = [&maxPerfCTCSpacingWithBoxes](uint64_t newSpacing) {
    if (maxPerfCTCSpacingWithBoxes.has_value()) {
      auto maxPerfMetalSpacing = maxPerfCTCSpacingWithBoxes.value();
      if (maxPerfMetalSpacing < newSpacing) {
        maxPerfCTCSpacingWithBoxes = std::make_optional(newSpacing);
      }
    } else {
      maxPerfCTCSpacingWithBoxes = std::make_optional(newSpacing);
    }
  };

  auto isHorizontal = layer->getDirection() == odb::dbTechLayerDir::HORIZONTAL;
  uint64_t width = layer->getWidth();
  uint64_t pitch = isHorizontal ? layer->getPitchY() : layer->getPitchX();
  uint64_t spacing = getMinSpacing(layer);
  if (DEBUG_VIAVIATRACKLENGTH) {
    printer.print(layer->getName(),
                  " calcViaViaTrackLength Via1 ",
                  viaX == nullptr ? "NullVia" : viaX->getViaName(),
                  " Via2 ",
                  viaY == nullptr ? "NullVia" : viaY->getViaName(),
                  " Pitch ",
                  pitch,
                  " Width ",
                  width);
  }
  auto metalBoxPair = getConstrainedMetalBoxes(layer, viaX, viaY);
  if (metalBoxPair.has_value()) {
    auto [metalBox1, metalBox2] = metalBoxPair.value();
    auto metalSpacing = getMaxRoutingPrlSpacing(layer, isSameNet);
    int64_t perfMetalSpacing;
    int64_t trackMetalDist
        = pitch
          - (isHorizontal ? getBoxCenterToTopEdge(metalBox1)
                                + getBoxCenterToBottomEdge(metalBox2)
                          : getBoxCenterToRightEdge(metalBox1)
                                + getBoxCenterToLeftEdge(metalBox2));
    if (trackMetalDist < 0) {
      perfMetalSpacing = metalSpacing;
    } else {
      perfMetalSpacing = std::ceil(std::sqrt(std::pow((int64_t) metalSpacing, 2)
                                             - std::pow(trackMetalDist, 2)));
    }

    if (perfMetalSpacing > 0) {
      perfMetalSpacing
          += (isHorizontal ? getBoxCenterToRightEdge(metalBox1)
                                 + getBoxCenterToLeftEdge(metalBox2)
                           : getBoxCenterToTopEdge(metalBox1)
                                 + getBoxCenterToBottomEdge(metalBox2));
      updateCTCSpacing(perfMetalSpacing);
    }
    if (DEBUG_VIAVIATRACKLENGTH) {
      printer.print("\tPerfMetalSpacing ", perfMetalSpacing);
    }
  }

  auto cutsBboxOfViaX = viaX->getCutsBBox();
  auto cutsBboxOfViaY = viaY->getCutsBBox();
  decltype(width) trackCutBBoxPadding
      = isHorizontal ? getBoxCenterToTopEdge(cutsBboxOfViaX)
                           + getBoxCenterToBottomEdge(cutsBboxOfViaY)
                     : getBoxCenterToRightEdge(cutsBboxOfViaX)
                           + getBoxCenterToLeftEdge(cutsBboxOfViaY);
  decltype(width) lineCutBBoxPadding
      = isHorizontal ? getBoxCenterToRightEdge(cutsBboxOfViaX)
                           + getBoxCenterToLeftEdge(cutsBboxOfViaY)
                     : getBoxCenterToTopEdge(cutsBboxOfViaX)
                           + getBoxCenterToBottomEdge(cutsBboxOfViaY);

  int64_t trackCutDist = pitch - trackCutBBoxPadding;

  for (auto [isCenterToCenter, prlOptional, spacing] :
       getCutSpacings(viaX, viaY, isSameNet)) {
    int64_t perfCutSpacing;
    auto prl = prlOptional.has_value() ? prlOptional.value() : 0;
    if (isCenterToCenter) {
      if (trackCutDist < 0) {
        perfCutSpacing = (-trackCutDist < prl)
                             ? lineCutBBoxPadding
                             : std::max(spacing, lineCutBBoxPadding);
      } else {
        if (spacing <= pitch) {
          perfCutSpacing = 0;
        } else {
          perfCutSpacing
              = std::ceil(std::sqrt(std::pow(spacing, 2) - std::pow(pitch, 2)));
          perfCutSpacing
              = std::min(perfCutSpacing, ((int64_t) lineCutBBoxPadding) - prl);
        }
      }
    } else {
      if (trackCutDist < 0) {
        perfCutSpacing = (-trackCutDist < prl) ? lineCutBBoxPadding
                                               : spacing + lineCutBBoxPadding;
      } else {
        if (spacing <= trackCutDist) {
          perfCutSpacing = 0;
        } else {
          perfCutSpacing = std::ceil(
              std::sqrt(std::pow(spacing, 2) - std::pow(trackCutDist, 2)));
          perfCutSpacing
              = std::min((int64_t) (perfCutSpacing + lineCutBBoxPadding),
                         ((int64_t) lineCutBBoxPadding) - prl);
        }
      }
    }
    if (perfCutSpacing > 0) {
      updateCTCSpacing(perfCutSpacing);
    }
    if (DEBUG_VIAVIATRACKLENGTH) {
      printer.print("\t",
                    " TrackCutBBoxPadding ",
                    trackCutBBoxPadding,
                    " LineCutBBoxPadding ",
                    lineCutBBoxPadding,
                    " TrackCutDist ",
                    trackCutDist,
                    " CutSpacing ",
                    isCenterToCenter,
                    prlOptional,
                    spacing,
                    " PerfCutSpacing ",
                    perfCutSpacing);
    }
  }
  auto rst = maxPerfCTCSpacingWithBoxes.has_value()
                 ? maxPerfCTCSpacingWithBoxes.value()
                 : 0;
  if (DEBUG_VIAVIATRACKLENGTH) {
    printer.print("\tresult ", rst);
  }
  return rst;
}

uint64_t Axioms::calcViaTrackLength(odb::dbTechLayer* layer,
                                    std::shared_ptr<Via> viaX,
                                    std::shared_ptr<Via> viaY,
                                    bool isSameNet)
{
  assert(layer->getType() == odb::dbTechLayerType::ROUTING);
  assert(viaX != nullptr && viaY == nullptr && !isSameNet);
  assert(layer->getDirection() != odb::dbTechLayerDir::NONE);

  auto layerDir = layer->getDirection();
  auto isHorizontal = layerDir == odb::dbTechLayerDir::HORIZONTAL;
  uint64_t width = layer->getWidth();

  auto metalBoxOfVia = viaX->getMetalBox(layer).value();
  auto prlSpacing = getMaxRoutingPrlSpacing(layer, isSameNet);

  uint64_t boxWidth
      = isHorizontal ? getBoxDy(metalBoxOfVia) : getBoxDx(metalBoxOfVia);

  auto rst = std::max(boxWidth, width) / 2 + width / 2 + prlSpacing;

  if (DEBUG_VIATRACKLENGTH) {
    printer.print(layer->getName(),
                  " calcViaTrackLength VIA ",
                  viaX->getViaName(),
                  " Width ",
                  width,
                  " PrlSpacing ",
                  prlSpacing,
                  " BoxWidth ",
                  boxWidth,
                  " result ",
                  rst);
  }

  return rst;
}

void Axioms::calcAxiomsRules()
{
  std::function<bool(int, int)> excludeNullFilter
      = [](int viaXId, int viaYId) -> bool {
    return viaXId != NULL_VIA_IDX && viaYId != NULL_VIA_IDX;
  };

  std::function<bool(int, int)> viaNullFilter
      = [](int viaXId, int viaYId) -> bool {
    return viaXId != NULL_VIA_IDX && viaYId == NULL_VIA_IDX;
  };

  for (auto layer : db_->getTech()->getLayers()) {
    if (layer->getType() != odb::dbTechLayerType::ROUTING) {
      continue;
    }
    std::vector<std::pair<std::string, std::shared_ptr<Via>>> viasInLayer;
    getViasInLayer(layer, viasInLayer);
    int layerIdx = AssertFind(dbAdapter_.layerNameToIdxs_, layer->getName());
    for (auto [rType, clacFn] : calcFnMap) {
      switch (rType) {
        case AxiomsRuleType::TrunkMinLength:
          std::tie(dbAdapter_.layers_[layerIdx].trunk_min_len_begin,
                   dbAdapter_.layers_[layerIdx].trunk_min_len_end)
              = addTableTriples(layer, viasInLayer, clacFn, true);
          break;
        case AxiomsRuleType::TrunkDistLength:
          std::tie(dbAdapter_.layers_[layerIdx].trunk_dist_same_begin,
                   dbAdapter_.layers_[layerIdx].trunk_dist_same_end)
              = addTableTriples(layer, viasInLayer, clacFn, true);

          std::tie(dbAdapter_.layers_[layerIdx].trunk_dist_diff_begin,
                   dbAdapter_.layers_[layerIdx].trunk_dist_diff_end)
              = addTableTriples(layer, viasInLayer, clacFn, false);
          break;
        case AxiomsRuleType::ViaViaTrunkLength:
          std::tie(dbAdapter_.layers_[layerIdx].viavia_trunk_begin,
                   dbAdapter_.layers_[layerIdx].viavia_trunk_end)
              = addTableTriples(
                  layer, viasInLayer, clacFn, true, excludeNullFilter);
          break;
        case AxiomsRuleType::ViaViaTrackLength:
          std::tie(dbAdapter_.layers_[layerIdx].viavia_track_same_begin,
                   dbAdapter_.layers_[layerIdx].viavia_track_same_end)
              = addTableTriples(
                  layer, viasInLayer, clacFn, true, excludeNullFilter);

          std::tie(dbAdapter_.layers_[layerIdx].viavia_track_diff_begin,
                   dbAdapter_.layers_[layerIdx].viavia_track_diff_end)
              = addTableTriples(
                  layer, viasInLayer, clacFn, false, excludeNullFilter);
          break;
        case AxiomsRuleType::ViaTrackLength:
          std::tie(dbAdapter_.layers_[layerIdx].via_track_begin,
                   dbAdapter_.layers_[layerIdx].via_track_end)
              = addTableTriples(
                  layer, viasInLayer, clacFn, false, viaNullFilter);
          break;
      }
    }
  }
}

void Axioms::calcSpacingTable()
{
  std::vector<banyan_go::table_triple>& triples = dbAdapter_.tableTriples_;
  for (auto layer : db_->getTech()->getLayers()) {
    if (layer->getType() != odb::dbTechLayerType::ROUTING) {
      continue;
    }
    int layerIdx = AssertFind(dbAdapter_.layerNameToIdxs_, layer->getName());
    dbAdapter_.layers_[layerIdx].spacing_begin = triples.size();

    auto routingTblRules = laToRoutingSpacingTableRules[layer];
    assert(routingTblRules.size() <= 1);
    for (auto rule : routingTblRules) {
      rule->widthAndPrlToSpacing([&triples](uint32_t width,
                                            uint32_t prl,
                                            uint32_t spacing) {
        triples.push_back({(int) prl + 1, (int) width + 1, (uint64_t) spacing});
      });
    }

    dbAdapter_.layers_[layerIdx].spacing_end = triples.size();
  }
}

void Axioms::calcMinimumCut()
{
  std::vector<banyan_go::table_triple>& triples = dbAdapter_.tableTriples_;
  for (auto layer : db_->getTech()->getLayers()) {
    if (layer->getType() != odb::dbTechLayerType::ROUTING) {
      continue;
    }
    int layerIdx = AssertFind(dbAdapter_.layerNameToIdxs_, layer->getName());
    dbAdapter_.layers_[layerIdx].mincut_begin = triples.size();
    std::vector<odb::dbTechMinCutRule*> cut_rules;
    if (layer->getMinimumCutRules(cut_rules)) {
      uint width;
      uint numcuts;
      uint length;
      uint distance;
      for (auto mincut_rule : cut_rules) {
        if (mincut_rule->getMinimumCuts(numcuts, width)) {
          if (mincut_rule->getLengthForCuts(length, distance)) {
            triples.push_back({(int) width, (int) distance, (uint64) numcuts});
          } else {
            triples.push_back({(int) width, 0, (uint64) numcuts});
          }
        }
      }
    }
    dbAdapter_.layers_[layerIdx].mincut_end = triples.size();
  }
}

void Axioms::calcDefaultSpacing()
{
  for (auto layer : db_->getTech()->getLayers()) {
    if (layer->getType() != odb::dbTechLayerType::ROUTING) {
      continue;
    }
    int layerIdx = AssertFind(dbAdapter_.layerNameToIdxs_, layer->getName());
    auto minSpacing = getMinSpacing(layer);
    auto defEOLPrlSpacing = getMaxRoutingEOLPrlSpacing(layer, nullptr, false);

    printer.print(layer->getName(),
                  " MinSpacing ",
                  minSpacing,
                  " EOLPrlSpacing ",
                  defEOLPrlSpacing);
    dbAdapter_.layers_[layerIdx].min_spacing = minSpacing;
    dbAdapter_.layers_[layerIdx].def_eol_prl_spacing = defEOLPrlSpacing;
  }
}

}  // namespace banyan
