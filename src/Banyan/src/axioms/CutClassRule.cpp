#include "axioms/CutClassRule.hpp"

namespace banyan {

LayerCutClassRules<CutClassRule> CutClassRule::laToCutClassRules = {};

std::optional<std::shared_ptr<CutClassRule>> CutClassRule::findCutClass(
    odb::dbTechLayer* layer,
    std::string className)
{
  assert(layer->getType() == odb::dbTechLayerType::CUT);
  auto cutClassesForLayer = CutClassRule::laToCutClassRules[layer];
  auto cutClassIter = cutClassesForLayer.find(className);
  if (cutClassIter == cutClassesForLayer.end()) {
    return std::nullopt;
  }

  return std::make_optional(cutClassIter->second);
}

bool CutClassRule::contain(std::shared_ptr<Via> via)
{
  assert(via != nullptr);
  auto cutsBBox = via->getCutsBBox();
  auto dx = getBoxDx(cutsBBox);
  auto dy = getBoxDx(cutsBBox);
  auto [boxMin, boxMax] = std::minmax(dx, dy);
  return boxMin == width && boxMax == length;
}

}  // namespace banyan