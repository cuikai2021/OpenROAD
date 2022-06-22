#pragma once

#include <assert.h>
#include <inttypes.h>

#include <boost/algorithm/string.hpp>
#include <optional>
#include <utility>
#include <variant>

#include "magic_enum.hpp"
#include "odb/db.h"
#include "utils.hpp"

namespace banyan {

class RoutingEOLSpacingRuleToNotchLength
{
 public:
  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingEOLSpacingRuleToNotchLength& obj)
  {
    out << " ToNotchLength (Unsupport Rule)";
    return out;
  }
};

class RoutingEOLSpacingRuleToConcaveCorner
{
 public:
  friend std::ostream& operator<<(
      std::ostream& out,
      const RoutingEOLSpacingRuleToConcaveCorner& obj)
  {
    out << " ToConcaveCorner (Unsupport Rule)";
    return out;
  }
};

class RoutingEOLSpacingRuleWithinEndToEnd
{
 private:
  uint32_t endToEndSpace;

 public:
  RoutingEOLSpacingRuleWithinEndToEnd(uint32_t endToEndSpace)
      : endToEndSpace(endToEndSpace){};
  ~RoutingEOLSpacingRuleWithinEndToEnd(){};

  uint32_t getEndToEndSpace() const { return endToEndSpace; }

  friend std::ostream& operator<<(
      std::ostream& out,
      const RoutingEOLSpacingRuleWithinEndToEnd& obj)
  {
    out << " ENDTOEND " << obj.endToEndSpace;
    return out;
  }
};

enum class EncloseCutPostion
{
  BELOW,
  ABOVE
};

class RoutingEOLSpacingRuleWithinEncloseCut
{
 private:
  std::optional<EncloseCutPostion> position;
  uint32_t encloseDist;
  uint32_t cutToMetalSpacing;
  bool isAllCuts;

 public:
  RoutingEOLSpacingRuleWithinEncloseCut(
      std::optional<EncloseCutPostion> position,
      uint32_t encloseDist,
      uint32_t cutToMetalSpacing,
      bool isAllCuts)
      : position(position),
        encloseDist(encloseDist),
        cutToMetalSpacing(cutToMetalSpacing),
        isAllCuts(isAllCuts){};

  ~RoutingEOLSpacingRuleWithinEncloseCut(){};

  friend std::ostream& operator<<(
      std::ostream& out,
      const RoutingEOLSpacingRuleWithinEncloseCut& obj)
  {
    out << " ENCLOSECUT " << obj.encloseDist;
    if (obj.position.has_value()) {
      out << " " << magic_enum::enum_name(obj.position.value()) << " ";
    }
    out << " CUTSPACING " << obj.cutToMetalSpacing;
    if (obj.isAllCuts) {
      out << " ALLCUTS ";
    }

    return out;
  }
};

class RoutingEOLSpacingRuleWithinPrl
{
 private:
  bool isSubtractEolWidth;
  uint32_t prlSpacing;
  uint32_t prlWithin;
  std::optional<uint32_t> minLength;
  bool isTwoEdges;
  bool isSameMetal;

 public:
  RoutingEOLSpacingRuleWithinPrl(bool isSubtractEolWidth,
                                 uint32_t prlSpacing,
                                 uint32_t prlWithin,
                                 std::optional<uint32_t> minLength,
                                 bool isTwoEdges,
                                 bool isSameMetal)
      : isSubtractEolWidth(isSubtractEolWidth),
        prlSpacing(prlSpacing),
        prlWithin(prlWithin),
        minLength(minLength),
        isTwoEdges(isTwoEdges),
        isSameMetal(isSameMetal){};

  ~RoutingEOLSpacingRuleWithinPrl(){};

  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingEOLSpacingRuleWithinPrl& obj)
  {
    out << " PARALLELEDGE ";
    if (obj.isSubtractEolWidth) {
      out << " SUBTRACTEOLWIDTH ";
    }
    out << obj.prlSpacing << " WITHIN " << obj.prlWithin;
    if (obj.minLength.has_value()) {
      out << " MINLENGTH " << obj.minLength.value();
    }
    if (obj.isTwoEdges) {
      out << " TWOEDGES ";
    }
    if (obj.isSameMetal) {
      out << " SAMEMETAL ";
    }
    return out;
  }
};

class RoutingEOLSpacingRuleWithin
{
 private:
  uint32_t eolWithin;
  std::optional<RoutingEOLSpacingRuleWithinEndToEnd> endToEnd;
  std::optional<RoutingEOLSpacingRuleWithinEncloseCut> encloseCut;
  std::optional<RoutingEOLSpacingRuleWithinPrl> prl;

 public:
  RoutingEOLSpacingRuleWithin(
      uint32_t eolWithin,
      std::optional<RoutingEOLSpacingRuleWithinEndToEnd> endToEnd,
      std::optional<RoutingEOLSpacingRuleWithinEncloseCut> encloseCut,
      std::optional<RoutingEOLSpacingRuleWithinPrl> prl)
      : eolWithin(eolWithin),
        endToEnd(endToEnd),
        encloseCut(encloseCut),
        prl(prl){};

  ~RoutingEOLSpacingRuleWithin(){};

  std::optional<RoutingEOLSpacingRuleWithinPrl> getPrl() const { return prl; }

  std::optional<RoutingEOLSpacingRuleWithinEncloseCut> getEncloseCut() const
  {
    return encloseCut;
  }

  std::optional<RoutingEOLSpacingRuleWithinEndToEnd> getEndToEnd() const
  {
    return endToEnd;
  }

  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingEOLSpacingRuleWithin& obj)
  {
    out << " WITHIN " << obj.eolWithin;
    if (obj.endToEnd.has_value()) {
      out << obj.endToEnd.value();
    }
    if (obj.prl.has_value()) {
      out << obj.prl.value();
    }
    if (obj.encloseCut.has_value()) {
      out << obj.encloseCut.value();
    }
    return out;
  }
};

class RoutingEOLSpacingRule
{
 private:
  odb::dbTechLayer* layer;
  uint32_t eolSpace;
  uint32_t eolWidth;
  std::variant<std::monostate,
               RoutingEOLSpacingRuleWithin,
               RoutingEOLSpacingRuleToConcaveCorner,
               RoutingEOLSpacingRuleToNotchLength>
      subRule;

 public:
  RoutingEOLSpacingRule(odb::dbTechLayer* layer,
                        uint32_t eolSpace,
                        uint32_t eolWidth,
                        uint32_t eolWithin,
                        bool prlEdge,
                        uint32_t prlSpace,
                        uint32_t prlWithin,
                        bool twoEdge)
      : layer(layer)
  {
    eolSpace = eolSpace;
    eolWidth = eolWidth;
    subRule = RoutingEOLSpacingRuleWithin(
        eolWithin,
        std::nullopt,
        std::nullopt,
        prlEdge ? std::make_optional(RoutingEOLSpacingRuleWithinPrl(
            false, prlSpace, prlWithin, std::nullopt, twoEdge, false))
                : std::nullopt);
  }

  RoutingEOLSpacingRule(odb::dbTechLayer* layer,
                        odb::dbTechLayerSpacingEolRule* rule)
      : layer(layer)
  {
    eolSpace = rule->getEolSpace();
    eolWidth = rule->getEolWidth();

    assert(!rule->isToNotchLengthValid() && !rule->isToConcaveCornerValid());

    auto eolWithin = rule->getEolWithin();
    auto endToEnd
        = rule->isEndToEndValid() ? std::make_optional(
              RoutingEOLSpacingRuleWithinEndToEnd(rule->getEndToEndSpace()))
                                  : std::nullopt;

    std::optional<EncloseCutPostion> encloseCutPos = std::nullopt;
    if (rule->isAboveValid()) {
      encloseCutPos = EncloseCutPostion::ABOVE;
    } else if (rule->isBelowValid()) {
      encloseCutPos = EncloseCutPostion::BELOW;
    }
    auto encloseCut
        = rule->isEncloseCutValid() ? std::make_optional(
              RoutingEOLSpacingRuleWithinEncloseCut(encloseCutPos,
                                                    rule->getEncloseDist(),
                                                    rule->getCutToMetalSpace(),
                                                    rule->isAllCutsValid()))
                                    : std::nullopt;

    auto prlEdge = rule->isParallelEdgeValid()
                       ? std::make_optional(RoutingEOLSpacingRuleWithinPrl(
                           rule->isSubtractEolWidthValid(),
                           rule->getParSpace(),
                           rule->getParWithin(),
                           rule->isParMinLengthValid()
                               ? std::make_optional(rule->getParMinLength())
                               : std::nullopt,
                           rule->isTwoEdgesValid(),
                           rule->isSameMetalValid()))
                       : std::nullopt;

    subRule
        = RoutingEOLSpacingRuleWithin(eolWithin, endToEnd, encloseCut, prlEdge);
  };

  ~RoutingEOLSpacingRule(){};

  std::variant<std::monostate,
               RoutingEOLSpacingRuleWithin,
               RoutingEOLSpacingRuleToConcaveCorner,
               RoutingEOLSpacingRuleToNotchLength>
  getSubRule() const
  {
    return subRule;
  }

  uint32_t getEolSpace() const { return eolSpace; }

  uint32_t getEolWidth() const { return eolWidth; }

  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingEOLSpacingRule& obj)
  {
    out << obj.layer->getName() << "\tSpacing " << obj.eolSpace << " ENDOFLINE "
        << obj.eolWidth;
    auto srValue = obj.subRule;
    std::visit(overloaded{
                   [](std::monostate&) {},
                   [&out](RoutingEOLSpacingRuleWithin& within) {
                     out << "\n\t" << within;
                   },
                   [&out](RoutingEOLSpacingRuleToConcaveCorner& corner) {
                     out << "\n\t" << corner;
                   },
                   [&out](RoutingEOLSpacingRuleToNotchLength& notchLen) {
                     out << "\n\t" << notchLen;
                   },
               },
               srValue);
    out << "\n";
    return out;
  }
};

}  // namespace banyan
