#include "axioms/CutSpacingRule.hpp"

namespace banyan {

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

bool CutSpacing::conditionValid(bool isSameNet,
                                odb::dbTechLayer* fromLayer,
                                odb::dbTechLayer* toLayer)
{
  auto isValid = true;

  if (!isSameNet && isSameNetRule()) {
    isValid = false;
  }

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