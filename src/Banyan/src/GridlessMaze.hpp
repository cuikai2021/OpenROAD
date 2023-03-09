#pragma once

#include <boost/assign.hpp>
#include <boost/foreach.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <string>
#include <tuple>
#include <vector>

#include "MergeWireSegment.hpp"
#include "PinCluster.hpp"
#include "odb/ZException.h"
#include "odb/db.h"
#include "odb/dbShape.h"
#include "odb/dbTransform.h"
#include "odb/dbWireCodec.h"
#include "odb/geom.h"
#include "thread_pool/ThreadPool.h"

namespace banyan {

using GmMazeNodeHashVal = std::tuple<int, int, int>;

struct GmRoutLine
{
  std::string layerName;
  int lx, ly, rx, ry;
  bool isLevel() const { return ly == ry; }
  GmBox extendBox(int x) const { return {lx - x, ly - x, rx + x, ry + x}; }
};

struct PIIHash
{
  size_t operator()(const std::pair<int, int>& p) const
  {
    return std::hash<int>()(p.first) ^ std::hash<int>()(p.second);
  }
};

struct GmMazeNodeHash
{
  size_t operator()(const GmMazeNodeHashVal& w) const
  {
    size_t res = std::hash<int>()(std::get<0>(w));
    res = (res << 5) ^ std::hash<int>()(std::get<1>(w));
    res = (res << 5) ^ std::hash<int>()(std::get<2>(w));
    return res;
  }
};

struct GmMazeNode
{
  int x, y, d;
  std::tuple<int, int> w;
  int sta, preDir;
  bool operator<(const GmMazeNode& r) const { return w > r.w; }
  bool operator==(const GmMazeNode& r) const
  {
    return x == r.x && y == r.y && sta == r.sta && preDir == r.preDir;
  }
};

struct GmVirtualPin
{
  std::string netName;
  std::vector<std::string> names;
  std::vector<GmRoutLine> lines;
};

class GridlessMaze
{
 public:
  using polygon = boost::polygon::polygon_45_data<int>;
  using point = boost::polygon::point_data<int>;
  using polygon_set = boost::polygon::polygon_45_set_data<int>;
  using DPoint
      = boost::geometry::model::d2::point_xy<int,
                                             boost::geometry::cs::cartesian>;
  using DBox = boost::geometry::model::box<DPoint>;
  using Value = std::pair<DBox, unsigned>;
  using PII = std::pair<int, int>;
  using RTree = boost::geometry::index::
      rtree<Value, boost::geometry::index::quadratic<100000>>;

 public:
  enum SearchLimitLevel
  {
    LOW = 100,
    MEDIUM = 500,
    HIGH = 2000,
    NOLIMIT = INT_MAX
  };
  GridlessMaze(odb::dbDatabase* db,
               utl::Logger* logger,
               SearchLimitLevel searchLimitLevel = MEDIUM)
      : logger_(logger), db_(db), searchLimit_(searchLimitLevel){};
  ~GridlessMaze() = default;
  std::vector<GmVirtualPin> Run();
  static void updateDB(odb::dbDatabase* db,
                       const std::vector<GmVirtualPin>& vpins);
  static DBox getDBoxByBox(const GmBox& b);
  static DBox getDBoxByPoint(int x1, int y1, int x2, int y2);
  static void cutTrapezoid(std::vector<GmBox>& boxes);

 private:
  bool isInTree(const RTree& rt, int x, int y);
  bool isInTree(const RTree& rt,int x1, int y1, int x2, int y2);
  std::vector<GmVirtualPin> ConnectPin(
      const PinCluster& pc,
      const std::unordered_map<std::string, std::string>& pin2net);
  std::vector<GmRoutLine> findPath(const std::vector<PII>& source,
                                   const std::vector<PII>& sink,
                                   std::vector<GmBox> sourceBox,
                                   std::vector<GmBox> sinkBox,
                                   const std::vector<int>& xList,
                                   const std::vector<int>& yList,
                                   const RTree& obsTree,
                                   const RTree& pinTree);
  void RemoveIllegal(std::vector<GmVirtualPin>& ans);
  utl::Logger* logger_;
  odb::dbDatabase* db_;
  int gridU_{}, U_{}, lineWidth_{}, maxInstWidth_{}, manufacturingGrid_{1};
  SearchLimitLevel searchLimit_{MEDIUM};
};

}  // namespace banyan