#pragma once

#include <assert.h>
#include <inttypes.h>

#include <boost/algorithm/string.hpp>
#include <optional>
#include <utility>
#include <variant>

#include "odb/db.h"
#include "utils.hpp"

namespace banyan {

class RoutingSpacingNotchLength
{
 private:
  uint32_t notchLength;

 public:
  RoutingSpacingNotchLength(uint32_t notchLength) : notchLength(notchLength){};
  ~RoutingSpacingNotchLength(){};
  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingSpacingNotchLength& obj)
  {
    out << "\t\tNOTCHLENGTH " << obj.notchLength;
    return out;
  }
};
class RoutingSpacingSameNet
{
 private:
  bool isPGOnly;

 public:
  RoutingSpacingSameNet(bool isPGOnly) : isPGOnly(isPGOnly){};
  ~RoutingSpacingSameNet(){};
  bool getIsPGOnly() const { return isPGOnly; }
  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingSpacingSameNet& obj)
  {
    out << "\t\tSAMENET ";
    if (obj.isPGOnly) {
      out << " PGONLY ";
    }
    return out;
  }
};

class RoutingSpacingRuleRange
{
 private:
  uint32_t minWidth;
  uint32_t maxWidth;

 public:
  RoutingSpacingRuleRange(uint32_t minWidth, uint32_t maxWidth)
      : minWidth(minWidth), maxWidth(maxWidth){};
  ~RoutingSpacingRuleRange(){};

  uint32_t getMinWidth() const { return minWidth; }

  uint32_t getMaxWidth() const { return maxWidth; }

  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingSpacingRuleRange& obj)
  {
    out << "\t\tRANGE " << obj.minWidth << " " << obj.maxWidth;
    return out;
  }
};

class RoutingSpacingRule
{
 private:
  odb::dbTechLayer* layer;
  uint32_t spacing;
  std::optional<std::variant<RoutingSpacingRuleRange,
                             RoutingSpacingSameNet,
                             RoutingSpacingNotchLength>>
      subRule;

 public:
  RoutingSpacingRule(odb::dbTechLayer* layer, odb::dbTechLayerSpacingRule* rule)
      : layer(layer)
  {
    spacing = rule->getSpacing();
    uint minRange;
    uint maxRange;
    if (rule->hasRange() && rule->getRange(minRange, maxRange)) {
      subRule = std::make_optional(RoutingSpacingRuleRange(minRange, maxRange));
      return;
    }

    if (rule->getSameNetPgOnly()) {
      subRule
          = std::make_optional(RoutingSpacingSameNet(rule->getSameNetPgOnly()));
      return;
    }

    if (rule->hasSpacingNotchLength()) {
      assert(false);
      return;
    }
  };
  ~RoutingSpacingRule(){};

  std::optional<std::variant<RoutingSpacingRuleRange,
                             RoutingSpacingSameNet,
                             RoutingSpacingNotchLength>>
  getSubRule() const
  {
    return subRule;
  }

  uint32_t getSpacing() const { return spacing; }

  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingSpacingRule& obj)
  {
    out << obj.layer->getName() << "\tSpacing " << obj.spacing;
    if (obj.subRule.has_value()) {
      auto srValue = obj.subRule.value();
      std::visit(overloaded{
                     [&out](RoutingSpacingRuleRange& range) {
                       out << "\n\t" << range;
                     },
                     [&out](RoutingSpacingSameNet& sameNet) {
                       out << "\n\t" << sameNet;
                     },
                     [&out](RoutingSpacingNotchLength& notchLen) {
                       out << "\n\t" << notchLen;
                     },
                 },
                 srValue);
    }
    return out;
  };
};

}  // namespace banyan