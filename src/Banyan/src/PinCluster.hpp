#pragma once
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/adapted/boost_polygon.hpp>
#include <fstream>
#include <utility>
#include <vector>

#include "odb/ZException.h"
#include "odb/db.h"
#include "odb/dbShape.h"
#include "odb/dbTransform.h"
#include "odb/dbWireCodec.h"
#include "odb/geom.h"
#include "utl/Logger.h"

namespace banyan {

class GmBox
{
 public:
  enum Type
  {
    PIN,
    OBS
  };

 public:
  GmBox()
  {
    xMin = yMin = xMax = yMax = 0;
    layer = nullptr;
    name = "";
    type = OBS;
  }
  GmBox(int xMin_,
        int yMin_,
        int xMax_,
        int yMax_,
        odb::dbTechLayer* layer_ = nullptr,
        std::string name_ = "",
        Type type_ = OBS)
      : xMin(xMin_),
        yMin(yMin_),
        xMax(xMax_),
        yMax(yMax_),
        layer(layer_),
        type(type_),
        name(std::move(name_)){};
  explicit GmBox(const odb::Rect& r,
                 odb::dbTechLayer* layer = nullptr,
                 std::string name = "",
                 Type type = OBS);
  ~GmBox() = default;
  static GmBox GetAbsoluteGMBox(int xMin,
                                int yMin,
                                int xMax,
                                int yMax,
                                const odb::dbTransform& transformer,
                                odb::dbTechLayer* layer,
                                std::string name = "",
                                Type type = OBS);
  bool contains(const GmBox& r) const
  {
    return (xMin <= r.xMin) && (yMin <= r.yMin) && (xMax >= r.xMax)
           && (yMax >= r.yMax);
  }

  bool contains(int x, int y) const
  {
    return xMin <= x && x <= xMax && yMin <= y && y <= yMax;
  }

  bool intersect(const GmBox& r) const
  {
    if (std::max(xMin, r.xMin) <= std::min(xMax, r.xMax)
        && std::max(yMin, r.yMin) <= std::min(yMax, r.yMax)) {
      return true;
    }
    return false;
  }

  bool operator<(const GmBox& r) const
  {
    if (xMin != r.xMin)
      return xMin < r.xMin;
    if (yMin != r.yMin)
      return yMin < r.yMin;
    if (xMax != r.xMax)
      return xMax < r.xMax;
    return yMax < r.yMax;
  }

  void expand(int w)
  {
    xMin -= w;
    yMin -= w;
    xMax += w;
    yMax += w;
  }

  void narrow(int w)
  {
    xMin += w;
    yMin += w;
    xMax -= w;
    yMax -= w;
  }

  void narrowNS(int w)
  {
    yMin += w;
    yMax -= w;
  }

  int xMin, yMin, xMax, yMax;
  odb::dbTechLayer* layer;
  Type type;
  std::string name;
};

struct GmPin
{
  GmPin(std::vector<GmBox> boxes_, std::string name_)
      : boxes(std::move(boxes_)), name(std::move(name_)){};

  std::vector<GmBox> boxes;
  std::string name;
};

class PinCluster
{
 public:
  PinCluster(int xMin,
             int yMin,
             int xMax,
             int yMax,
             const std::vector<GmBox>& boxes)
      : xMin_(xMin), yMin_(yMin), xMax_(xMax), yMax_(yMax), boxes_(boxes){};
  int xMin() const { return xMin_; }
  void setXMin(int xMin) { xMin_ = xMin; }
  int yMin() const { return yMin_; }
  void setYMin(int yMin) { yMin_ = yMin; }
  int xMax() const { return xMax_; }
  void setXMax(int xMax) { xMax_ = xMax; }
  int yMax() const { return yMax_; }
  void setYMax(int yMax) { yMax_ = yMax; }
  const std::vector<GmBox>& boxes() const { return boxes_; }
  void setBoxes(const std::vector<GmBox>& boxes) { boxes_ = boxes; }

 private:
  int xMin_, yMin_, xMax_, yMax_;
  std::vector<GmBox> boxes_;
};

struct GmLine
{
  GmLine(int lx_, int ly_, int rx_, int ry_)
      : lx(lx_), ly(ly_), rx(rx_), ry(ry_){};
  int lx, ly, rx, ry;
};

struct GmPointEvent
{
  enum stu
  {
    IN,
    OUT
  };
  GmPointEvent(int x_, int y_, int id_, stu s_)
      : x(x_), y(y_), id(id_), s(s_){};
  int x, y, id;
  stu s;
  bool operator<(const GmPointEvent& r) const { return y < r.y; }
};

class PinClusterBuilder
{
 public:
  using polygon = boost::polygon::polygon_45_data<int>;
  using point = boost::polygon::point_data<int>;
  using polygon_set = boost::polygon::polygon_45_set_data<int>;

 public:
  PinClusterBuilder(odb::dbDatabase* db, utl::Logger* logger)
      : db_(db), logger_(logger){};
  ~PinClusterBuilder() = default;
  std::vector<PinCluster> Build();
  static odb::dbTechLayer* FindFirstRoutingLayer(odb::dbTech* dbTech);
  static std::vector<GmBox> GetRow(const GmBox& mb, std::vector<GmBox> pows);
  static std::vector<std::vector<GmBox>> GetRowBoxes(
      const std::vector<GmBox>& rows,
      const std::vector<GmBox>& boxes);
  static std::string PinName(odb::dbITerm* dbIterm);
  static std::string PinNameInst(std::string s);
  static std::string PinNamePin(std::string s);
  static polygon GetRectPoly(int x1, int y1, int x2, int y2);

 private:
  std::vector<PinCluster> GetPinClusters(const GmBox& row,
                                         const std::vector<GmBox>& rowBoxes);
  std::vector<polygon> GetObsPolyGon(
      const std::vector<GmBox>& rows,
      const std::vector<std::vector<GmBox>>& rowBoxes);
  odb::dbDatabase* db_;
  utl::Logger* logger_;
};

class GmDefDrawer
{
 public:
  GmDefDrawer()
  {
    xMax_ = yMax_ = 100000;
    init();
  }
  GmDefDrawer(int xMin, int yMin, int xMax, int yMax)
      : xMin_(xMin), yMin_(yMin), xMax_(xMax), yMax_(yMax)
  {
    init();
  }
  GmDefDrawer(int xMax, int yMax) : xMax_(xMax), yMax_(yMax) { init(); }
  ~GmDefDrawer() = default;

  void drawBoxes(const std::string& layer, const std::vector<GmBox>& boxes)
  {
    std::string ans;
    char buf[1000];
    for (const auto& box : boxes) {
      std::sprintf(buf,
                   "- LAYER %s RECT ( %d %d ) ( %d %d ) ;\n",
                   layer.c_str(),
                   box.xMin,
                   box.yMin,
                   box.xMax,
                   box.yMax);
      ans += std::string(buf);
    }
    text_.emplace_back(ans);
  }

  void drawPolygon(const std::string& layer,
                   const std::vector<PinClusterBuilder::polygon>& polygons)
  {
    std::string ans;
    for (const auto& p : polygons) {
      std::vector<std::pair<int, int>> tmp;
      for (const auto& x : p) {
        tmp.emplace_back(x.x(), x.y());
      }
      tmp.pop_back();
      std::string s = "- LAYER " + layer + " POLYGON ";
      for (const auto& [x, y] : tmp) {
        s += std::to_string(x) + " " + std::to_string(y) + " ";
      }
      s += ";\n";
      ans += s;
    }
    text_.emplace_back(ans);
  }

  void drawLine(const std::string& info) { text_.emplace_back(info); }

  bool write(const std::string& path)
  {
    std::fstream file;
    file.open(path, std::ios::out);  // write,清空再写入
    if (file.is_open()) {
      file << head_;
      std::sort(text_.begin(), text_.end());
      for (auto& s : text_)
        file << s;
      file << tail_;
      return true;
    }
    return false;
  }

 private:
  void init()
  {
    head_ = "DESIGN CORTEXM3 ;\n";
    char buf[256];
    std::sprintf(
        buf, "DIEAREA ( %d %d ) ( %d %d ) ;\n", xMin_, yMin_, xMax_, yMax_);
    head_ += std::string(buf);
    head_ += "BLOCKAGES 1 ;\n";
    tail_ = "END BLOCKAGES\nEND DESIGN\n";
  }
  int xMin_{}, yMin_{}, xMax_{}, yMax_{};
  std::string head_, tail_;
  std::vector<std::string> text_;
};

}  // namespace banyan