#pragma once

#include <optional>

#include <assert.h>
#include <inttypes.h>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/adapted/boost_polygon.hpp>
#include <boost/polygon/polygon.hpp>

#include "odb/db.h"

namespace banyan {

namespace gtl = boost::polygon;
using namespace boost::polygon::operators;

using Rectangle = gtl::rectangle_data<int64_t>;
using Polygon90 = gtl::polygon_90_with_holes_data<int64_t>;
using Polygon90Set = gtl::polygon_90_set_data<int64_t>;

class Via
{
 private:
  odb::dbTechVia* viaInODB;
  odb::dbTechLayer* viaLayer;
  Rectangle topLayerBox;
  Rectangle bottomLayerBox;
  Polygon90Set cuts;
  uint32_t cutsNum;

 public:
  Via(odb::dbTechVia* via) : viaInODB(via)
  {
    uint32_t vCutsNum = 0;
    for (auto box : via->getBoxes()) {
      if (box->getTechLayer() == via->getTopLayer()) {
        topLayerBox
            = Rectangle(box->xMin(), box->yMin(), box->xMax(), box->yMax());
      } else if (box->getTechLayer() == via->getBottomLayer()) {
        bottomLayerBox
            = Rectangle(box->xMin(), box->yMin(), box->xMax(), box->yMax());
      } else {
        viaLayer = box->getTechLayer();
        cuts += Rectangle(box->xMin(), box->yMin(), box->xMax(), box->yMax());
        vCutsNum++;
      }
    }
    cutsNum = vCutsNum;
  };

  ~Via(){};

  friend std::ostream& operator<<(std::ostream& out, const Via& obj)
  {
    out << obj.viaLayer->getName() << " " << obj.viaInODB->getName();
    if (obj.viaInODB->isDefault()) {
      out << " DEFAULT\n";
    }

    out << " LAYER " << obj.viaInODB->getTopLayer()->getName() << " ;\n";
    auto topLL = ll(obj.topLayerBox);
    auto topUR = ur(obj.topLayerBox);
    out << "\tRECT " << topLL.x() << " " << topLL.y() << " " << topUR.x() << " "
        << topUR.y() << "\n";

    out << " LAYER " << obj.viaLayer->getName() << " ;\n";
    std::vector<Rectangle> rects;
    get_rectangles(rects, obj.cuts);
    for (auto rect : rects) {
      auto rLL = ll(rect);
      auto rUR = ur(rect);
      out << "\tRECT " << rLL.x() << " " << rLL.y() << " " << rUR.x() << " "
          << rUR.y() << "\n";
    }

    out << " LAYER " << obj.viaInODB->getBottomLayer()->getName() << " ;\n";
    auto bottomLL = ll(obj.bottomLayerBox);
    auto bottomUR = ur(obj.bottomLayerBox);
    out << "\tRECT " << bottomLL.x() << " " << bottomLL.y() << " "
        << bottomUR.x() << " " << bottomUR.y() << "\n";

    return out;
  }

  std::string getViaName() noexcept { return viaInODB->getName(); }

  odb::dbTechLayer* topLayer() noexcept;

  odb::dbTechLayer* bottomLayer() noexcept;

  std::optional<Rectangle> getMetalBox(odb::dbTechLayer* layer) noexcept;

  Rectangle getCutsBBox() noexcept;

  odb::dbTechLayer* getViaLayer() const { return viaLayer; }
};

}  // namespace banyan