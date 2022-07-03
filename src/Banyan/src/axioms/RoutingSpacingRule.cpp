#include "axioms/RoutingSpacingRule.hpp"

namespace banyan {

std::optional<uint32_t> RoutingSpacingRule::getLengthThreshold()
{
    if(!subRule.has_value() || subRule.value().index()!= 3){
        return std::nullopt;
    }
    auto lengthThreshold = std::get<RoutingSpacingLengthThreshold>(subRule.value());
    return std::make_optional(lengthThreshold.getMaxLength());
}

std::tuple<uint32_t, uint32_t, uint32_t>
RoutingSpacingRange::getMinMaxWidthAndPrl(
    std::optional<uint32_t> lengthThreshold)
{
  auto minMax1 = std::make_pair(minWidth, maxWidth);
  std::tuple<uint32_t, uint32_t, uint32_t> rst;
  if (subRule.has_value()) {
    rst = std::make_tuple(minMax1.first, minMax1.second, 0);
  }

  auto greaterRange = [](std::pair<uint32_t, uint32_t> range1,
                         std::pair<uint32_t, uint32_t> range2) {
    assert(range1.second <= range2.first || range2.second <= range1.first);
    return range1.second <= range2.first ? range2 : range1;
  };

  auto srValue = subRule.value();
  std::visit(
      overloaded{
          [&](RoutingSpacingRangeUseLengthThreshold& useLenThreshold) {
            rst = std::make_tuple(
                minMax1.first, minMax1.second, lengthThreshold.value_or(0));
          },
          [&](RoutingSpacingRangeInfluence& rangeInfluence) {
            auto stubMinMax = rangeInfluence.getStubMinMaxWidth().value_or(
                std::make_pair(0, 0));
            auto range = greaterRange(minMax1, stubMinMax);
            rst = std::make_tuple(range.first, range.second, 0);
          },
          [&](RoutingSpacingRangeRange& rangeRange) {
            auto range = greaterRange(minMax1, rangeRange.getRange());
            rst = std::make_tuple(range.first, range.second, 0);
          },
      },
      srValue);
  return rst;
}

}  // namespace banyan