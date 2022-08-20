#pragma once

#include <assert.h>
#include <inttypes.h>

#include <functional>
#include <optional>
#include <variant>
#include <ostream>

#include "magic_enum.hpp"
#include "odb/db.h"
#include "utils.hpp"

namespace banyan {

class CutSpacingSameMask
{
 public:
  friend std::ostream& operator<<(std::ostream& out,
                                  const CutSpacingSameMask& obj)
  {
    out << " SameMask (Unsupport Rule)";
    return out;
  }
};

class CutSpacingMAXXY
{
 public:
  friend std::ostream& operator<<(std::ostream& out, const CutSpacingMAXXY& obj)
  {
    out << " MAXXY (Unsupport Rule)";
    return out;
  }
};

enum class CutSpacingValidCondition
{
  SAMENET,
  SAMEMETAL,
  SAMEVIA
};

class CutSpacingLayerStack
{
 private:
  odb::dbTechLayer* toLayer;
  bool stack;

 public:
  CutSpacingLayerStack(odb::dbTechLayer* toLayer, bool stack)
      : toLayer(toLayer), stack(stack){};
  ~CutSpacingLayerStack(){};
  odb::dbTechLayer* getToLayer() const { return toLayer; }
  friend std::ostream& operator<<(std::ostream& out,
                                  const CutSpacingLayerStack& obj)
  {
    out << " LAYER " << obj.toLayer->getName();
    if (obj.stack) {
      out << " STACK ";
    }
    return out;
  }
};

enum class AdjacentCutsNum : uint32_t
{
  TWO = 2,
  THREE = 3,
  FOUR = 4
};

class CutSpacingAdjacentCuts
{
 private:
  AdjacentCutsNum cutsNum;
  uint32_t within1;
  std::optional<uint32_t> within2;

 public:
  CutSpacingAdjacentCuts(AdjacentCutsNum cutsNum, uint32_t within1)
      : cutsNum(cutsNum), within1(within1), within2(std::nullopt){};
  ~CutSpacingAdjacentCuts(){};
  friend std::ostream& operator<<(std::ostream& out,
                                  const CutSpacingAdjacentCuts& obj)
  {
    out << " ADJACENTCUTS " << (uint32_t) obj.cutsNum;
    out << " WITHIN " << obj.within1;
    if (obj.within2.has_value()) {
      out << "  " << obj.within2.value();
    }
    return out;
  }
};

enum class CutSpacingParallelOverlapExceptType
{
  EXCEPTSAMENET,
  EXCEPTSAMEMETAL,
  EXCEPTSAMEMETALOVERLAP,
  EXCEPTSAMEVIA
};

class CutSpacingParallelOverlap
{
 private:
  std::optional<CutSpacingParallelOverlapExceptType> exceptType;

 public:
  CutSpacingParallelOverlap() : exceptType(std::nullopt){};
  CutSpacingParallelOverlap(
      std::optional<CutSpacingParallelOverlapExceptType> exceptType)
      : exceptType(exceptType){};
  ~CutSpacingParallelOverlap(){};

  bool isExceptSameNet();

  friend std::ostream& operator<<(std::ostream& out,
                                  const CutSpacingParallelOverlap& obj)
  {
    out << " PARALLELOVERLAP ";
    if (obj.exceptType.has_value()) {
      out << " " << magic_enum::enum_name(obj.exceptType.value()) << " ";
    }
    return out;
  }
};

class CutSpacingParallelWithin
{
 private:
  uint32_t within;
  bool isExceptSameNet;

 public:
  CutSpacingParallelWithin(uint32_t within)
      : within(within), isExceptSameNet(false){};
  ~CutSpacingParallelWithin(){};
  friend std::ostream& operator<<(std::ostream& out,
                                  const CutSpacingParallelWithin& obj)
  {
    out << " PARALLELWITHIN " << obj.within;
    if (obj.isExceptSameNet) {
      out << " EXCEPTSAMENET ";
    }
    return out;
  }
};

class CutSpacing
{
 private:
  bool centerToCenter;
  std::optional<CutSpacingValidCondition> validCondition;
  std::optional<std::variant<CutSpacingLayerStack,
                             CutSpacingAdjacentCuts,
                             CutSpacingParallelOverlap,
                             CutSpacingParallelWithin>>
      subRule;

 public:
  CutSpacing(bool centerToCenter,
             std::optional<CutSpacingValidCondition> validCondition,
             std::optional<std::variant<CutSpacingLayerStack,
                                        CutSpacingAdjacentCuts,
                                        CutSpacingParallelOverlap,
                                        CutSpacingParallelWithin>> subRule)
      : centerToCenter(centerToCenter),
        validCondition(validCondition),
        subRule(subRule){};

  ~CutSpacing(){};

  bool getCenterToCenter() const { return centerToCenter; }

  bool isSameNetRule();

  bool isExceptSameNetRule();

  bool isParallelOverlapRule();

  bool isAdjacentcutsRule();

  std::optional<CutSpacingValidCondition> getValidCondition() const
  {
    return validCondition;
  }

  std::optional<std::variant<CutSpacingLayerStack,
                             CutSpacingAdjacentCuts,
                             CutSpacingParallelOverlap,
                             CutSpacingParallelWithin>>
  getSubRule() const
  {
    return subRule;
  }

  bool constrained(odb::dbTechLayer* fromLayer,
                      odb::dbTechLayer* toLayer);

  friend std::ostream& operator<<(std::ostream& out, const CutSpacing& obj)
  {
    if (obj.centerToCenter) {
      out << " CENTERTOCENTER ";
    }
    if (obj.validCondition.has_value()) {
      out << " " << magic_enum::enum_name(obj.validCondition.value()) << " ";
    }
    if (obj.subRule.has_value()) {
      auto srValue = obj.subRule.value();
      std::visit(
          overloaded{
              [&out](CutSpacingLayerStack& ls) { out << "\t" << ls; },
              [&out](CutSpacingAdjacentCuts& ac) { out << "\t" << ac; },
              [&out](CutSpacingParallelOverlap& po) { out << "\t" << po; },
              [&out](CutSpacingParallelWithin& pw) { out << "\t" << pw; },
          },
          srValue);
    }
    return out;
  }
};

class CutSpacingRule
{
 private:
  odb::dbTechLayer* layer;
  uint32_t spacing;
  std::optional<std::variant<CutSpacingSameMask, CutSpacingMAXXY, CutSpacing>>
      subRule;

 public:
  CutSpacingRule(odb::dbTechLayer* layer, odb::dbTechLayerSpacingRule* rule)
      : layer(layer)
  {
    spacing = rule->getSpacing();
    auto centerToCenter = rule->getCutCenterToCenter();
    auto sameNet = rule->getCutSameNet();
    auto validCondition
        = sameNet ? std::make_optional(CutSpacingValidCondition::SAMENET)
                  : (std::nullopt);

    odb::dbTechLayer* toLayer = nullptr;
    auto hasToLayer = rule->getCutLayer4Spacing(toLayer);
    if (hasToLayer) {
      auto stack = rule->getCutStacking();
      subRule = CutSpacing(
          centerToCenter,
          validCondition,
          std::make_optional(CutSpacingLayerStack(toLayer, stack)));
      return;
    }

    uint numcuts = 0;
    uint within = 0;
    uint adjCutsSpacing = 0;
    bool execptSamePGNet = false;
    auto hasAdjacentcuts = rule->getAdjacentCuts(
        numcuts, within, adjCutsSpacing, execptSamePGNet);
    assert(execptSamePGNet == false);
    if (hasAdjacentcuts) {
      spacing = adjCutsSpacing;
      subRule = CutSpacing(centerToCenter,
                           validCondition,
                           std::make_optional(CutSpacingAdjacentCuts(
                               AdjacentCutsNum(numcuts), within)));
      return;
    }

    auto prlOverlap = rule->getCutParallelOverlap();
    if (prlOverlap) {
      subRule = CutSpacing(centerToCenter,
                           validCondition,
                           std::make_optional(CutSpacingParallelOverlap()));
      return;
    }

    auto cutArea = rule->getCutArea();
    assert(cutArea == 0);
  };

  CutSpacingRule(odb::dbTechLayer* layer, odb::dbTechLayerCutSpacingRule* rule)
      : layer(layer)
  {
    spacing = rule->getCutSpacing();

    auto validConditionMapFn = [](odb::dbTechLayerCutSpacingRule* rule)
        -> std::optional<CutSpacingValidCondition> {
      if (rule->isSameNet()) {
        return std::make_optional(CutSpacingValidCondition::SAMENET);
      } else if (rule->isSameMetal()) {
        return std::make_optional(CutSpacingValidCondition::SAMEMETAL);
      } else if (rule->isSameVia()) {
        return std::make_optional(CutSpacingValidCondition::SAMEVIA);
      }

      return std::nullopt;
    };

    auto toLayer = rule->getSecondLayer();
    switch (rule->getType()) {
      case odb::dbTechLayerCutSpacingRule::NONE:
        spacing = rule->getCutSpacing();
        break;
      case odb::dbTechLayerCutSpacingRule::MAXXY:
        assert(false);
        break;
      case odb::dbTechLayerCutSpacingRule::SAMEMASK:
        assert(false);
        break;
      case odb::dbTechLayerCutSpacingRule::LAYER:

        subRule = CutSpacing(rule->isCenterToCenter(),
                             validConditionMapFn(rule),
                             toLayer == nullptr
                                 ? std::nullopt
                                 : std::make_optional(CutSpacingLayerStack(
                                     toLayer, rule->isStack())));
        break;
      case odb::dbTechLayerCutSpacingRule::ADJACENTCUTS:
        subRule = CutSpacing(
            rule->isCenterToCenter(),
            validConditionMapFn(rule),
            std::make_optional(CutSpacingAdjacentCuts(
                AdjacentCutsNum(rule->getAdjacentCuts()), rule->getWithin())));

        assert(!rule->isTwoCutsValid());
        assert(rule->getSecondWithin() <= 0);
        assert(!rule->isExceptSamePgnet());
        assert(rule->getCutClass() == nullptr);
        assert(!rule->isNoPrl());
        assert(!rule->isCutClassToAll());
        assert(!rule->isSameMask());
        break;
      case odb::dbTechLayerCutSpacingRule::PARALLELOVERLAP:
        subRule = CutSpacing(
            rule->isCenterToCenter(),
            validConditionMapFn(rule),
            std::make_optional(
                rule->isExceptSameNet()
                    ? CutSpacingParallelOverlap(std::make_optional(
                        CutSpacingParallelOverlapExceptType::EXCEPTSAMENET))
                    : CutSpacingParallelOverlap()));

        assert(!rule->isExceptSameMetal() && !rule->isExceptSameMetalOverlap()
               && !rule->isExceptSameVia());
        break;
      case odb::dbTechLayerCutSpacingRule::PARALLELWITHIN:
        assert(false);
        break;
      case odb::dbTechLayerCutSpacingRule::SAMEMETALSHAREDEDGE:
        assert(false);
        break;
      case odb::dbTechLayerCutSpacingRule::AREA:
        assert(false);
        break;
    }
  };

  ~CutSpacingRule(){};

  friend std::ostream& operator<<(std::ostream& out, const CutSpacingRule& obj)
  {
    out << obj.layer->getName() << "\tCUTSPACING " << obj.spacing;
    if (obj.subRule.has_value()) {
      auto srValue = obj.subRule.value();
      std::visit(
          overloaded{
              [&out](CutSpacingSameMask& smask) { out << "\t" << smask; },
              [&out](CutSpacingMAXXY& maxxy) { out << "\t" << maxxy; },
              [&out](CutSpacing& spacing) { out << "\t" << spacing; },
          },
          srValue);
    }
    return out;
  }

  uint64_t getSpacing() { return spacing; }

  bool hasSubRule() { return subRule.has_value(); }

  std::optional<std::variant<CutSpacingSameMask, CutSpacingMAXXY, CutSpacing>>
  getSubRule()
  {
    return subRule;
  }
};

}  // namespace banyan
