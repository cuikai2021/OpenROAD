#include "axioms/CutSpacingRule.hpp"

namespace banyan {

bool CutSpacingParallelOverlap::isExceptSameNet()
{
  if (exceptType.has_value()
      && exceptType.value()
             == CutSpacingParallelOverlapExceptType::EXCEPTSAMENET) {
    return true;
  }
  return false;
}

bool CutSpacing::isParallelOverlapRule()
{
  return subRule.has_value() && subRule.value().index() == 2;
}

bool CutSpacing::isAdjacentcutsRule()
{
  return subRule.has_value() && subRule.value().index() == 1;
}

bool CutSpacing::isSameNetRule()
{
  return validCondition.has_value()
         && (validCondition.value() == CutSpacingValidCondition::SAMENET
             || validCondition.value() == CutSpacingValidCondition::SAMEMETAL);
}

bool CutSpacing::isExceptSameNetRule()
{
  if (subRule.has_value() && subRule.value().index() == 2) {
    return std::get<CutSpacingParallelOverlap>(subRule.value())
        .isExceptSameNet();
  }
  return false;
}

bool CutSpacing::constrained(odb::dbTechLayer* fromLayer,
                             odb::dbTechLayer* toLayer)
{
  auto isValid = true;

  if (subRule.has_value() && subRule.value().index() == 0
      && std::get<CutSpacingLayerStack>(subRule.value()).getToLayer()
             != toLayer) {
    isValid = false;
  }

  if (fromLayer != toLayer
      && (!subRule.has_value() || subRule.value().index() != 0
          || std::get<CutSpacingLayerStack>(subRule.value()).getToLayer()
                 != toLayer)) {
    isValid = false;
  }

  return isValid;
}
}  // namespace banyan