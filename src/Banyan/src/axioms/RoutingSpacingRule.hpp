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

class RoutingSpacingRangeRange
{
 private:
  uint64_t minWidth;
  uint64_t maxWidth;

 public:
  RoutingSpacingRangeRange(uint32_t minWidth, uint32_t maxWidth)
      : minWidth(minWidth), maxWidth(maxWidth){};
  ~RoutingSpacingRangeRange(){};
  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingSpacingRangeRange& obj)
  {
    out << " RANGE " << obj.minWidth << " " << obj.maxWidth;
    return out;
  }
};

class RoutingSpacingRangeUseLengthThreshold
{
 public:
  friend std::ostream& operator<<(
      std::ostream& out,
      const RoutingSpacingRangeUseLengthThreshold& obj)
  {
    out << " USELENGTHTHRESHOLD ";
    return out;
  }
};

class RoutingSpacingRangeInfluence
{
 private:
  uint64_t influenceLength;
  std::optional<std::pair<uint64_t, uint64_t>> stubMinMaxWidth;

 public:
  RoutingSpacingRangeInfluence(
      uint64_t influenceLength,
      std::optional<std::pair<uint64_t, uint64_t>> stubMinMaxWidth)
      : influenceLength(influenceLength), stubMinMaxWidth(stubMinMaxWidth){};
  ~RoutingSpacingRangeInfluence(){};
  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingSpacingRangeInfluence& obj)
  {
    out << " INFLUENCE " << obj.influenceLength;
    if (obj.stubMinMaxWidth.has_value()) {
      auto [minW, maxW] = obj.stubMinMaxWidth.value();
      out << " RANGE " << minW << " " << maxW;
    }
    return out;
  }
};

class RoutingSpacingRange
{
 private:
  uint32_t minWidth;
  uint32_t maxWidth;
  std::optional<std::variant<RoutingSpacingRangeUseLengthThreshold,
                             RoutingSpacingRangeInfluence,
                             RoutingSpacingRangeRange>>
      subRule;

 public:
  RoutingSpacingRange(uint32_t minWidth, uint32_t maxWidth)
      : minWidth(minWidth), maxWidth(maxWidth){};

  RoutingSpacingRange(uint32_t minWidth,
                      uint32_t maxWidth,
                      bool useLengthThreshold)
      : minWidth(minWidth), maxWidth(maxWidth)
  {
    assert(useLengthThreshold);
    subRule = std::make_optional(RoutingSpacingRangeUseLengthThreshold());
  };

  RoutingSpacingRange(uint32_t minWidth,
                      uint32_t maxWidth,
                      RoutingSpacingRangeInfluence subRule)
      : minWidth(minWidth),
        maxWidth(maxWidth),
        subRule(std::make_optional(subRule)){};

  RoutingSpacingRange(uint32_t minWidth,
                      uint32_t maxWidth,
                      RoutingSpacingRangeRange subRule)
      : minWidth(minWidth),
        maxWidth(maxWidth),
        subRule(std::make_optional(subRule)){};

  ~RoutingSpacingRange(){};

  uint32_t getMinWidth() const { return minWidth; }

  uint32_t getMaxWidth() const { return maxWidth; }

  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingSpacingRange& obj)
  {
    out << " RANGE " << obj.minWidth << " " << obj.maxWidth;

    if (obj.subRule.has_value()) {
      auto srValue = obj.subRule.value();
      std::visit(
          overloaded{
              [&out](RoutingSpacingRangeUseLengthThreshold& useLenThreshold) {
                out << "\n\t" << useLenThreshold;
              },
              [&out](RoutingSpacingRangeInfluence& rangeInfluence) {
                out << "\n\t" << rangeInfluence;
              },
              [&out](RoutingSpacingRangeRange& rangeRange) {
                out << "\n\t" << rangeRange;
              },
          },
          srValue);
    }
    return out;
  }
};

class RoutingSpacingLengthThreshold
{
 private:
  uint64_t maxLength;
  std::optional<std::pair<uint64_t, uint64_t>> minMaxWidth;

 public:
  RoutingSpacingLengthThreshold(
      uint64_t maxLength,
      std::optional<std::pair<uint64_t, uint64_t>> minMaxWidth)
      : maxLength(maxLength), minMaxWidth(minMaxWidth){};
  ~RoutingSpacingLengthThreshold(){};
  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingSpacingLengthThreshold& obj)
  {
    out << " LENGTHTHRESHOLD " << obj.maxLength;
    if (obj.minMaxWidth.has_value()) {
      auto [minW, maxW] = obj.minMaxWidth.value();
      out << " RANGE " << minW << " " << maxW;
    }

    return out;
  }
};

class RoutingSpacingRule
{
 private:
  odb::dbTechLayer* layer;
  uint32_t spacing;
  std::optional<std::variant<RoutingSpacingRange,
                             RoutingSpacingSameNet,
                             RoutingSpacingNotchLength,
                             RoutingSpacingLengthThreshold>>
      subRule;

 public:
  RoutingSpacingRule(odb::dbTechLayer* layer, odb::dbTechLayerSpacingRule* rule)
      : layer(layer)
  {
    spacing = rule->getSpacing();
    uint minRange;
    uint maxRange;
    if (rule->hasRange() && rule->getRange(minRange, maxRange)) {
      if (rule->hasUseLengthThreshold()) {
        subRule
            = std::make_optional(RoutingSpacingRange(minRange, maxRange, true));
        return;
      }

      uint rangeMin;
      uint rangeMax;
      if (rule->getRangeRange(rangeMin, rangeMax)) {
        subRule = std::make_optional(RoutingSpacingRange(
            minRange, maxRange, RoutingSpacingRangeRange(rangeMin, rangeMax)));
        return;
      }

      uint lengthInfluence;
      if (rule->getInfluence(lengthInfluence)) {
        std::optional<std::pair<uint64_t, uint64_t>> influenceMinMax;

        uint influenceMin;
        uint influenceMax;
        if (rule->getInfluenceRange(influenceMin, influenceMax)) {
          influenceMinMax = std::make_optional(
              std::make_pair((uint64_t) influenceMin, (uint64_t) influenceMax));
        }
        subRule = std::make_optional(RoutingSpacingRange(
            minRange,
            maxRange,
            RoutingSpacingRangeInfluence(lengthInfluence, influenceMinMax)));

        return;
      }

      subRule = std::make_optional(RoutingSpacingRange(minRange, maxRange));
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

    if (rule->hasLengthThreshold()) {
      uint lengthThreshold;
      if (rule->getLengthThreshold(lengthThreshold)) {
        std::optional<std::pair<uint64_t, uint64_t>> lenThresholdMinMax;

        uint lenThresholdMin;
        uint lenThresholdMax;
        if (rule->getLengthThresholdRange(lenThresholdMin, lenThresholdMax)) {
          lenThresholdMinMax = std::make_optional(std::make_pair(
              (uint64_t) lenThresholdMin, (uint64_t) lenThresholdMax));
        }
        subRule = std::make_optional(
            RoutingSpacingLengthThreshold(lengthThreshold, lenThresholdMinMax));
        return;
      }
    }
  };
  ~RoutingSpacingRule(){};

  std::optional<std::variant<RoutingSpacingRange,
                             RoutingSpacingSameNet,
                             RoutingSpacingNotchLength,
                             RoutingSpacingLengthThreshold>>
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
      std::visit(
          overloaded{
              [&out](RoutingSpacingRange& range) { out << "\n\t" << range; },
              [&out](RoutingSpacingSameNet& sameNet) {
                out << "\n\t" << sameNet;
              },
              [&out](RoutingSpacingNotchLength& notchLen) {
                out << "\n\t" << notchLen;
              },
              [&out](RoutingSpacingLengthThreshold& lenThreshold) {
                out << "\n\t" << lenThreshold;
              },
          },
          srValue);
    }
    return out;
  }
};

}  // namespace banyan