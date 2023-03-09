#pragma once

#include <memory>
#include <vector>

#include "linden/geom/point.hpp"
#include "linden/geom/rect.hpp"
#include "nesterovBase.h"

namespace odb {
class dbDatabase;
}

namespace grt {
class GlobalRouter;
}

namespace utl {
class Logger;
}

namespace linden {

class Tile : public linden::geom::Rect, public linden::geom::Point
{
 public:
  Tile() = delete;
  Tile(int x, int y, int lx, int ly, int ux, int uy);
  Tile(const Tile& ano) = default;
  Tile(Tile&&) = default;
  Tile& operator=(const Tile&) = default;
  Tile& operator=(Tile&&) = default;
  ~Tile() = default;

  double inflationRatio() const noexcept { return _inflationRation; }
  double& inflationRatio() noexcept { return _inflationRation; }

  double inflatedRatio() const noexcept { return _inflatedRatio; }
  double& inflatedRatio() noexcept { return _inflatedRatio; }

 private:
  double _inflationRation;
  double _inflatedRatio;
};

class TileGrid
{
 public:
  TileGrid() = default;
  ~TileGrid() = default;

  int lx() const noexcept { return ll().x(); }
  int ly() const noexcept { return ll().y(); }
  int ux() const noexcept { return lx() + tileCnt().x() * tileSize().x(); }
  int uy() const noexcept { return ly() + tileCnt().y() * tileSize().y(); }

  const linden::geom::Point& ll() const noexcept { return _ll; }
  linden::geom::Point& ll() noexcept { return _ll; }

  const linden::geom::Point& tileCnt() const noexcept { return _tileCnt; }
  linden::geom::Point& tileCnt() noexcept { return _tileCnt; }

  const linden::geom::Point& tileSize() const noexcept { return _tileSize; }
  linden::geom::Point& tileSize() noexcept { return _tileSize; }

  int numRoutingLayers() const noexcept { return _numRoutingLayers; }
  int& numRoutingLayers() { return _numRoutingLayers; }

  const std::vector<Tile>& tiles() const noexcept { return _tiles; }
  std::vector<Tile>& tiles() noexcept { return _tiles; }

  void init();

 private:
  std::vector<Tile> _tiles;

  linden::geom::Point _ll;  // lower-left position
  linden::geom::Point _tileCnt;
  linden::geom::Point _tileSize;
  int _numRoutingLayers;
};

class RouteBaseVars
{
 public:
  float inflationRatioCoef;
  float maxInflationRatio;
  float maxDensity;
  float targetRC;
  float ignoreEdgeRatio;
  float minInflationRatio;

  // targetRC metric coefficients.
  float rcK1, rcK2, rcK3, rcK4;

  int maxBloatIter;
  int maxInflationIter;

  RouteBaseVars();
  void reset();
};

class RouteBase
{
 public:
  RouteBase();
  RouteBase(RouteBaseVars rbVars,
            odb::dbDatabase* db,
            grt::GlobalRouter* grouter,
            std::shared_ptr<gpl::NesterovBase> nb,
            utl::Logger* log);
  ~RouteBase();

  // update Route and Est info
  // from GlobalRouter
  void updateRoute();
  void getGlobalRouterResult();

  // first: is Routability Need
  // second: reverting procedure need in NesterovPlace
  //         (e.g. calling NesterovPlace's init())
  std::pair<bool, bool> routability();

  int64_t inflatedAreaDelta() const;
  int numCall() const;
  int bloatIterCnt() const;
  int inflationIterCnt() const;

  float getRC() const;

  void revertGCellSizeToMinRc();

 private:
  RouteBaseVars rbVars_;
  odb::dbDatabase* db_;
  grt::GlobalRouter* grouter_;

  std::shared_ptr<gpl::NesterovBase> nb_;
  utl::Logger* log_;

  std::unique_ptr<TileGrid> tg_;

  int64_t inflatedAreaDelta_;

  int bloatIterCnt_;
  int inflationIterCnt_;
  int numCall_;

  // if solutions are not improved at all,
  // needs to revert back to have the minimized RC values.
  // minRcInflationSize_ will store
  // GCell's width and height
  float minRc_;
  float minRcTargetDensity_;
  int minRcViolatedCnt_;
  std::vector<std::pair<int, int>> minRcCellSize_;

  void init();
  void reset();
  void resetRoutabilityResources();

  // update inflationIterCnt_, bloatIterCnt_ and numCall_
  void increaseCounter();

  // routability funcs
  void initGCells();
};

}  // namespace linden
