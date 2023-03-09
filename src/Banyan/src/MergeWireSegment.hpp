#pragma once

#include <algorithm>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/adapted/boost_polygon.hpp>
#include <deque>
#include <string>
#include <utility>
#include <variant>

#include "odb/ZException.h"
#include "odb/db.h"
#include "odb/dbShape.h"
#include "odb/dbTransform.h"
#include "odb/dbWireCodec.h"
#include "odb/geom.h"
#include "utl/Logger.h"

namespace banyan {

using polygon = boost::polygon::polygon_data<int>;
using point = boost::polygon::polygon_traits<polygon>::point_type;
using polygon_set = boost::polygon::polygon_set_data<int>;

class MwsPoint
{
 public:
  MwsPoint() { x_ = y_ = 0; }
  MwsPoint(int x, int y) : x_(x), y_(y){};

  void setX(int x) { x_ = x; }
  int x() const { return x_; }

  void setY(int y) { y_ = y; }
  int y() const { return y_; }
  void route() { std::swap(x_, y_); }

  bool operator==(const MwsPoint& r) { return x_ == r.x_ && y_ == r.y_; }

 private:
  int x_, y_;
};

class MwsLine
{
 public:
  MwsLine() = default;
  MwsLine(MwsPoint p1, MwsPoint p2, bool isMerged = false) : isMerged_(isMerged)
  {
    start_ = MwsPoint(std::min(p1.x(), p2.x()), std::min(p1.y(), p2.y()));
    end_ = MwsPoint(std::max(p1.x(), p2.x()), std::max(p1.y(), p2.y()));
  };

  void setEnd(MwsPoint end) { end_ = end; }
  void setStart(MwsPoint start) { start_ = start; }
  void setIsMerged(bool isMerged) { isMerged_ = isMerged; }

  MwsPoint start() { return start_; }
  MwsPoint end() { return end_; }
  bool isMerged() const { return isMerged_; }
  bool isLevel() { return start_.y() == end_.y(); }
  bool isPoint() { return start_ == end_; }
  static bool IsOverlapping(MwsLine lhs, MwsLine rhs);
  polygon extendRect();

  void route()
  {
    start_.route();
    end_.route();
  }

 private:
  MwsPoint start_, end_;
  bool isMerged_{};
};

class MwsRect
{
 public:
  MwsRect() = default;
  MwsRect(MwsPoint point, int xMin, int yMin, int xMax, int yMax)
      : point_(point), xMin_(xMin), yMin_(yMin), xMax_(xMax), yMax_(yMax){};

  void setPoint(MwsPoint p) { point_ = p; }
  MwsPoint point() { return point_; }

  void setXMin(int xMin) { xMin_ = xMin; }
  int xMin() const { return xMin_; }

  void setYMin(int yMin) { yMin_ = yMin; }
  int yMin() const { return yMin_; }

  void setXMax(int xMax) { xMax_ = xMax; }
  int xMax() const { return xMax_; }

  void setYMax(int yMax) { yMax_ = yMax; }
  int yMax() const { return yMax_; }

 private:
  MwsPoint point_;
  int xMin_{}, yMin_{}, xMax_{}, yMax_{};
};

class MwsVia
{
 public:
  MwsVia() = default;
  MwsVia(std::string name, const MwsPoint& point)
      : name_(std::move(name)), point_(point){};

  void setName(const std::string& name) { name_ = name; }
  std::string name() { return name_; }

  void setPoint(MwsPoint p) { point_ = p; }
  MwsPoint point() { return point_; }

  ~MwsVia() = default;

 private:
  std::string name_;
  MwsPoint point_;
};

class MwsWire
{
  using WireVal = std::variant<MwsLine, MwsRect, MwsVia>;
  enum WireType
  {
    LINE,
    RECT,
    VIA
  };

 public:
  MwsWire(std::string layerName, WireVal val)
      : layerName_(std::move(layerName)), val_(std::move(val)){};
  ~MwsWire() = default;
  void setLayerName(const std::string& layerName) { layerName_ = layerName; }
  std::string layerName() { return layerName_; }

  void setVal(const WireVal& val) { val_ = val; }

  bool isLine() { return val_.index() == LINE; }
  bool isRect() { return val_.index() == RECT; }
  bool isVia() { return val_.index() == VIA; }

  MwsLine line()
  {
    if (isLine()) {
      return std::get<LINE>(val_);
    }
    return {};
  }

  MwsRect rect()
  {
    if (isRect()) {
      return std::get<RECT>(val_);
    }
    return {};
  }

  MwsVia via()
  {
    if (isVia()) {
      return std::get<VIA>(val_);
    }
    return {};
  }

 private:
  std::string layerName_;
  WireVal val_;
};

class MergeWireSegment
{
 public:
  using netWireMap = std::unordered_map<std::string, std::deque<MwsWire>>;

 public:
  MergeWireSegment(odb::dbDatabase* db, utl::Logger* logger)
      : db_(db), logger_(logger){};
  void Run();
  static std::deque<MwsLine> mergeLines(std::deque<MwsLine> lines,
                                        bool isLevel);
  static std::deque<MwsWire> mergeWireLines(const std::deque<MwsWire>& wires);
  static void updateDB(odb::dbDatabase* db, const netWireMap& result);

 private:
  netWireMap GetWireFromDb();

 private:
  odb::dbDatabase* db_;
  utl::Logger* logger_;
};

}  // namespace banyan