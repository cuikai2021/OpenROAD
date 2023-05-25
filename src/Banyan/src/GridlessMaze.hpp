#pragma once
#include "ByAdapter.hpp"
#include "PinCluster.hpp"
namespace banyan {

using InstPinName = std::pair<std::string, std::string>;
using PII = std::pair<int, int>;
using GridlessMazeNodeHashVal = std::tuple<int, int, int>;

struct GmRoutingLine
{
  std::string layerName;
  std::string netName;
  int x1, y1, x2, y2;
  ByBox toBox(int lw) const
  {
    return ByBox(x1 - lw / 2,
                 y1 - lw / 2,
                 x2 + lw / 2,
                 y2 + lw / 2,
                 std::make_shared<ByBoxInfo>(layerName,
                                             netName,
                                             std::nullopt,
                                             std::nullopt,
                                             std::nullopt,
                                             ByBoxInfo::WIRE));
  }
};

struct GridlessMazeNodeHashFunc
{
  size_t operator()(const GridlessMazeNodeHashVal& w) const
  {
    size_t res = std::hash<int>()(std::get<0>(w));
    res = (res << 5) ^ std::hash<int>()(std::get<1>(w));
    res = (res << 5) ^ std::hash<int>()(std::get<2>(w));
    return res;
  }
};

struct GridlessMazeNode
{
  int x, y, dis;
  PII weight;
  int dirSta, preDir;
  bool operator<(const GridlessMazeNode& r) const { return weight > r.weight; }
};

struct pair_hash
{
  template <class T1, class T2>
  std::size_t operator()(const std::pair<T1, T2>& p) const
  {
    return std::hash<T1>()(p.first) ^ std::hash<T2>()(p.second);
  }
};

struct GmVirtualPin
{
  std::string netName;
  std::vector<InstPinName> names;
  std::vector<GmRoutingLine> lines;
};

class GridlessMaze
{
 public:
  using Rtree = bgi::rtree<ByBox, bgi::quadratic<100000>>;

 public:
  enum
  {
    WEST,
    SOUTH,
    EAST,
    NORTH
  };

  enum SEARCH_LIMIT
  {
    SMALL = 100,
    MEDIUN = 500,
    HIGH = 2500,
    NOLIMIT = std::numeric_limits<int>::max()
  };

 public:
  GridlessMaze(odb::dbDatabase* db,
               const std::shared_ptr<ByDb>& bydb,
               utl::Logger* logger,
               SEARCH_LIMIT searchLimit = MEDIUN);

 public:
  std::vector<GmVirtualPin> run();

 private:
  void markTouchWirePin(PinCluster& pc);

  std::vector<GmVirtualPin> solveOneRowPinCluster(
      std::vector<PinCluster>& pcs,
      const std::unordered_map<InstPinName, std::string, pair_hash>& pin2Net);

  std::vector<GmVirtualPin> solvePinCluster(
      PinCluster& pc,
      const std::unordered_map<InstPinName, std::string, pair_hash>& pin2Net);

  std::vector<int> getXVal(const ByBox& b);

  std::vector<int> getYVal(const ByBox& b);

  void getPinPoint(std::vector<PII>& ans,
                   const ByBox& b,
                   int x1,
                   int x2,
                   int y1,
                   int y2);

  void extendHanan(std::vector<int>& v,
                   const std::vector<int>& totPinVal,
                   int x1,
                   int x2);

  bool checkSimpleSource(const std::pair<InstPinName, std::vector<ByBox>>& lhs,
                         const std::pair<InstPinName, std::vector<ByBox>>& rhs);

  std::vector<ByBox> getRect(const std::vector<ByBox>& boxes,
                             bsp::orientation_2d ori);

  std::vector<GmRoutingLine> findPath(const std::string &netName,const std::vector<PII>& source,
                                      const std::vector<PII>& sink,
                                      std::vector<ByBox> sourceBox,
                                      std::vector<ByBox> sinkBox,
                                      const std::vector<int>& xList,
                                      const std::vector<int>& yList,
                                      const Rtree& obsTree,
                                      const Rtree& pinTree);

  bool checkDrcInPin(int x,
                     int y,
                     int dir,
                     const std::vector<ByBox>& vBoxes,
                     const std::vector<ByBox>& hBoxes,
                     bool isSink);

  bool checkLineDrc(const GmRoutingLine& line,
                    const std::vector<ByBox>& vBoxes,
                    const std::vector<ByBox>& hBoxes);

  bool checkTurnPointDrc(const BsPoint& p, const std::vector<ByBox>& boxes);

  bool isInBoxes(int x,
                 int y,
                 const std::vector<ByBox>& boxes,
                 bool touch = true);

  bool isInBoxesBoundary(int x, int y, const std::vector<ByBox>& boxes);

  bool checkOffGrid(int x, int y);

  bool isInRtree(const Rtree& rt, int x, int y);

  void initBigger(std::vector<int>& v, const std::vector<int>& a, int x);

  void initLess(std::vector<int>& v, const std::vector<int>& a, int x);

  void initBiggest(std::vector<int>& v, const std::vector<int>& a, int x);

  void initLeast(std::vector<int>& v, const std::vector<int>& a, int x);

  std::vector<ByBox> getMaxRect(const std::vector<ByBox>& boxes);

 private:
  odb::dbDatabase* db_;
  std::shared_ptr<ByDb> bydb_;
  utl::Logger* logger_;
  int spacing_{}, lineWidth_{}, manufacturingGrid_{1};
  int U_, GridU_;
  SEARCH_LIMIT searchLimit_;
};

}  // namespace banyan
