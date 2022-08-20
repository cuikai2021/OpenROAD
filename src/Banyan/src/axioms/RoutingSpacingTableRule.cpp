#include "axioms/RoutingSpacingTableRule.hpp"

#include <set>
namespace banyan {

void RoutingSpacingTableRule::widthAndPrlToSpacing(
    std::function<void(uint32_t width, uint32_t prl, uint32_t sapcing)>
        callback)
{
  assert(subRule.index() == 1);
  auto srValue = std::get<RoutingSpacingTableRulePrl>(subRule);

  for (auto width : widths) {
    for (auto prl : prls) {
      auto spacing = srValue.widthPrlToSpacings.at(std::make_pair(width, prl));
      callback(width, prl, spacing);
    }
  }
}

uint64_t RoutingSpacingTableRule::getMinSpacing()
{
  assert(subRule.index() == 1);
  auto srValue = std::get<RoutingSpacingTableRulePrl>(subRule);
  uint64_t minWidth = *widths.begin();
  uint64_t minPrl = *prls.begin();
  return srValue.widthPrlToSpacings.at(std::make_pair(minWidth, minPrl));
}

uint64_t RoutingSpacingTableRule::getSpacing(uint64_t width, uint64_t prl)
{
  assert(subRule.index() == 1);
  auto srValue = std::get<RoutingSpacingTableRulePrl>(subRule);

  auto searchLastSmaller
      = [](uint64_t value, std::set<uint32_t>& values) -> uint64_t {
    uint64_t rst = 0;
    for (auto v : values) {
      rst = v;
      if (value <= v) {
        break;
      }
    }
    return rst;
  };

  uint64_t gWidth = searchLastSmaller(width, widths);
  uint64_t gPrl = searchLastSmaller(prl, prls);
  return srValue.widthPrlToSpacings.at(std::make_pair(gWidth, gPrl));
}

}  // namespace banyan