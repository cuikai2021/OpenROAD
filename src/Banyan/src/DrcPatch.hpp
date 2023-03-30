#pragma once
#include <boost/polygon/polygon.hpp>
#include <unordered_map>

#include "odb/ZException.h"
#include "odb/db.h"
#include "odb/dbShape.h"
#include "odb/dbTransform.h"
#include "odb/dbWireCodec.h"
#include "odb/geom.h"
#include "utl/Logger.h"

namespace banyan {

namespace ply = boost::polygon;
using namespace boost::polygon::operators;

using PlyRect = ply::rectangle_data<int>;
using PlyPolygon = ply::polygon_90_with_holes_data<int>;
using PlyPolygonPoint = ply::polygon_traits<PlyPolygon>::point_type;
using PlyPolygonSet = ply::polygon_90_set_data<int>;
using PlyPolygonHole = ply::polygon_with_holes_traits<PlyPolygon>::hole_type;

class DrcPatch
{
 protected:
  odb::dbDatabase* db_;
  utl::Logger* logger_;

 public:
  DrcPatch(odb::dbDatabase* db, utl::Logger* logger)
      : db_(db), logger_(logger){};
  ~DrcPatch();
 

  PlyRect dbBox2PlyRect(const odb::dbTransform& transformer, odb::dbBox* dbBox);
  std::vector<PlyRect> dbWire2Rects(odb::dbTechLayer* layer,
                                    odb::dbWire* odbWire);

  const std::string getItermName(odb::dbITerm* iterm) const;

  std::vector<PlyRect>  sameNetRects(odb::dbTechLayer* layer,
                                       odb::dbNet* net);

  std::vector<PlyRect> spacingDrcPatch(odb::dbTechLayer* layer,
                                       odb::dbNet* net,
                                       std::vector<PlyRect> sameNetRects,
                                       std::vector<PlyRect> patchRects);

  std::vector<PlyRect> minHolePatch(odb::dbTechLayer* layer,
                                    odb::dbNet* net,
                                       std::vector<PlyRect> sameNetRects,
                                       std::vector<PlyRect> patchRects);

   void run();
};

}  // namespace banyan