#include "axioms/Via.hpp"

namespace banyan {

odb::dbTechLayer* Via::topLayer() noexcept
{
  return viaInODB->getTopLayer();
}

odb::dbTechLayer* Via::bottomLayer() noexcept
{
  return viaInODB->getBottomLayer();
}

std::optional<Rectangle> Via::getMetalBox(odb::dbTechLayer* layer) noexcept
{
  if (layer == topLayer()) {
    return std::make_optional(topLayerBox);
  } else if (layer == bottomLayer()) {
    return std::make_optional(bottomLayerBox);
  }
  return std::nullopt;
}

Rectangle Via::getCutsBBox() noexcept
{
  Rectangle bbox;
  cuts.extents(bbox);
  return bbox;
}

}  // namespace banyan