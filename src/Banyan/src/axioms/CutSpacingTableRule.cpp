#include "axioms/CutSpacingTableRule.hpp"

namespace banyan {

bool CutSpacingTableDefault::isSameNetRule()
{
  return validCondition.has_value()
         && (validCondition.value() == CutSpacingValidCondition::SAMENET
             || validCondition.value() == CutSpacingValidCondition::SAMEMETAL);
}

std::vector<std::tuple<bool, std::optional<int64_t>, uint64_t>>
CutSpacingTableRule::getCutSpacings(std::shared_ptr<Via> via1,
                                    std::shared_ptr<Via> via2,
                                    bool isSameNet)
{
  std::vector<std::tuple<bool, std::optional<int64_t>, uint64_t>> results;
  assert(subRule.index() == 2);

  auto tbl = std::get<CutSpacingTableDefault>(subRule);
  if (!tbl.toLayer.has_value() && via1->getViaLayer() != via2->getViaLayer()) {
    return results;
  }

  if (tbl.toLayer.has_value() && via2->getViaLayer() != tbl.toLayer.value()) {
    return results;
  }

  if (isSameNet != tbl.isSameNetRule()) {
    return results;
  }

  std::optional<int64_t> prl;
  if (tbl.prl.has_value()) {
    prl = std::make_optional(tbl.prl.value().prl);
  }

  for (auto item : tbl.items) {
    if ((item->getCutClass1().has_value()
         && !item->getCutClass1().value()->contain(via1))
        || (item->getCutClass2().has_value()
            && !item->getCutClass2().value()->contain(via2))) {
      continue;
    }
    results.push_back({false, prl, item->getCutSpacing2()});
    results.push_back({false, std::nullopt, item->getCutSpacing1()});
  }

  return results;
}
}  // namespace banyan