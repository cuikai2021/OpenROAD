#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include "grt/GlobalRouter.h"
#include "nesterovBase.h"
#include "odb/db.h"
#include "linden.rdp.h"
#include "utl/Logger.h"

using grt::GlobalRouter;
using std::make_pair;
using std::pair;
using std::sort;
using std::string;
using std::vector;

using utl::GPL;
using namespace gpl;

namespace linden {

void TileGrid::init()
{
  // log_->info(GPL, 1036, "TileLxLy: {} {}", lx(), ly());
  // log_->info(GPL, 1037, "TileSize: {} {}", tileSize().x(), tileSize().y());
  // log_->info(GPL, 1038, "TileCnt: {} {}", tileCnt().x(), tileCnt().y());
  // 2D tile grid structure init
  linden::geom::Point idx(0, 0);
  _tiles.reserve(tileCnt().x() * tileCnt().y());
  for (idx.y() = 0; idx.y() < tileCnt().y(); ++idx.y()) {
    auto y = ly() + idx.y() * tileSize().y();
    for (idx.x() = 0; idx.x() < tileCnt().x(); ++idx.x()) {
      auto x = lx() + idx.x() * tileSize().x();
      _tiles.emplace_back(Tile(idx.x(),
                               idx.y(),
                               x, y,
                               x + tileSize().x(),
                               y + tileSize().y()));
    }
  }
  // log_->info(GPL, 1040, "NumTiles: {}", tiles_.size());
}

/////////////////////////////////////////////
// RouteBaseVars

RouteBaseVars::RouteBaseVars()
{
  reset();
}

void RouteBaseVars::reset()
{
  inflationRatioCoef = 2.5;
  maxInflationRatio = 2.5;
  maxDensity = 0.90;
  targetRC = 1.25;
  ignoreEdgeRatio = 0.8;
  minInflationRatio = 1.01;
  rcK1 = rcK2 = 1.0;
  rcK3 = rcK4 = 0.0;
  maxBloatIter = 1;
  maxInflationIter = 4;
}

/////////////////////////////////////////////
// RouteBase

RouteBase::RouteBase()
    : rbVars_(),
      db_(nullptr),
      grouter_(nullptr),
      nb_(nullptr),
      log_(nullptr),
      inflatedAreaDelta_(0),
      bloatIterCnt_(0),
      inflationIterCnt_(0),
      numCall_(0),
      minRc_(1e30),
      minRcTargetDensity_(0),
      minRcViolatedCnt_(0)
{
}

RouteBase::RouteBase(RouteBaseVars rbVars,
                     odb::dbDatabase* db,
                     grt::GlobalRouter* grouter,
                     std::shared_ptr<NesterovBase> nb,
                     utl::Logger* log)
    : RouteBase()
{
  rbVars_ = rbVars;
  db_ = db;
  grouter_ = grouter;
  nb_ = nb;
  log_ = log;

  init();
}

RouteBase::~RouteBase()
{
}

void RouteBase::reset()
{
  rbVars_.reset();
  db_ = nullptr;
  nb_ = nullptr;
  log_ = nullptr;

  bloatIterCnt_ = inflationIterCnt_ = 0;
  numCall_ = 0;

  minRc_ = 1e30;
  minRcTargetDensity_ = 0;
  minRcViolatedCnt_ = 0;

  minRcCellSize_.clear();
  minRcCellSize_.shrink_to_fit();

  resetRoutabilityResources();
}

void RouteBase::resetRoutabilityResources()
{
  inflatedAreaDelta_ = 0;

  grouter_->clear();
  tg_.reset();
}

void RouteBase::init()
{
  // tg_ init
  std::unique_ptr<TileGrid> tg(new TileGrid());
  tg_ = std::move(tg);

  // tg_->setLogger(log_);
  minRcCellSize_.resize(nb_->gCells().size(), std::make_pair(0, 0));
}

void RouteBase::getGlobalRouterResult()
{
  // update gCells' location to DB for GR
  nb_->updateDbGCells();

  // these two options must be on
  grouter_->setAllowCongestion(true);
  grouter_->setOverflowIterations(0);

  grouter_->globalRoute();

  updateRoute();
}

int64_t RouteBase::inflatedAreaDelta() const
{
  return inflatedAreaDelta_;
}

int RouteBase::numCall() const
{
  return numCall_;
}

int RouteBase::bloatIterCnt() const
{
  return bloatIterCnt_;
}

int RouteBase::inflationIterCnt() const
{
  return inflationIterCnt_;
}

static float getUsageCapacityRatio(Tile& tile,
                                   odb::dbTechLayer* layer,
                                   odb::dbGCellGrid* gGrid,
                                   float ignoreEdgeRatio)
{
  unsigned int capH = 0, capV = 0, capU = 0;
  unsigned int useH = 0, useV = 0, useU = 0;
  unsigned int blockH = 0, blockV = 0, blockU = 0;
  gGrid->getCapacity(layer, tile.x(), tile.y(), capH, capV, capU);
  gGrid->getUsage(layer, tile.x(), tile.y(), useH, useV, useU);
  gGrid->getBlockage(layer, tile.x(), tile.y(), blockH, blockV, blockU);

  const bool isH = layer->getDirection() == odb::dbTechLayerDir::HORIZONTAL;

  unsigned int curCap = (isH) ? capH : capV;
  unsigned int curUse = (isH) ? useH : useV;
  unsigned int blockage = (isH) ? blockH : blockV;

  // escape tile ratio cals when capacity = 0
  if (curCap == 0) {
    return -1 * FLT_MAX;
  }

  // ignore if blockage is too huge in current tile
  float blockageRatio = static_cast<float>(blockage) / curCap;
  if (blockageRatio >= ignoreEdgeRatio) {
    return -1 * FLT_MAX;
  }

  // return usage (used routing track + blockage) / total capacity
  return static_cast<float>(curUse) / curCap;
}

// tile_grid.init and update its inflation info
void RouteBase::updateRoute()
{
  odb::dbGCellGrid* gGrid = db_->getChip()->getBlock()->getGCellGrid();
  std::vector<int> gridX, gridY;
  gGrid->getGridX(gridX);
  gGrid->getGridY(gridY);

  // retrieve routing Layer Count from odb
  odb::dbTech* tech = db_->getTech();
  int numLayers = tech->getRoutingLayerCount();

  // update grid tile info
  tg_->ll().x() = gridX[0];
  tg_->ll().y() = gridY[0];
  tg_->tileSize().x() = gridX[1] - gridX[0];
  tg_->tileSize().y() = gridY[1] - gridY[0];
  tg_->tileCnt().x() = gridX.size();
  tg_->tileCnt().y() = gridY.size();
  tg_->init();

  for (int i = 1; i <= numLayers; i++) {
    odb::dbTechLayer* layer = tech->findRoutingLayer(i);
    bool isHorizontalLayer
        = (layer->getDirection() == odb::dbTechLayerDir::HORIZONTAL);

    for (auto& tile : tg_->tiles()) {
      // Check left and down tile
      // and set the minimum usage/cap vals for
      // TileGrid setup.

      // first extract current tiles' usage
      float ratio
          = getUsageCapacityRatio(tile, layer, gGrid, rbVars_.ignoreEdgeRatio);

      // if horizontal layer (i.e., vertical edges)
      // should consider LEFT tile's RIGHT edge == current 'tile's LEFT edge
      // (current 'ratio' points to RIGHT edges usage)
      if (isHorizontalLayer && tile.x() >= 1) {
        Tile& leftTile
            = tg_->tiles()[tile.y() * tg_->tileCnt().x() + tile.x() - 1];
        float leftRatio = getUsageCapacityRatio(
            leftTile, layer, gGrid, rbVars_.ignoreEdgeRatio);
        ratio = fmax(leftRatio, ratio);
      }

      // if vertical layer (i.e., horizontal edges)
      // should consider DOWN tile's UP edge == current 'tile's DOWN edge
      // (current 'ratio' points to UP edges usage)
      if (!isHorizontalLayer && tile.y() >= 1) {
        Tile& downTile
            = tg_->tiles()[(tile.y() - 1) * tg_->tileCnt().x() + tile.x()];
        float downRatio = getUsageCapacityRatio(
            downTile, layer, gGrid, rbVars_.ignoreEdgeRatio);
        ratio = fmax(downRatio, ratio);
      }

      ratio = fmax(ratio, 0.0f);

      // update inflation Ratio
      if (ratio >= rbVars_.minInflationRatio) {
        float inflationRatio = pow(ratio, rbVars_.inflationRatioCoef);
        inflationRatio = fmin(inflationRatio, rbVars_.maxInflationRatio);
        tile.inflationRatio() = inflationRatio;
      }
    }
  }

  // debug print
  for (auto& tile : tg_->tiles()) {
    if (tile.inflationRatio() > 1.0) {
      debugPrint(log_,
                 GPL,
                 "replace",
                 5,
                 "updateInflationRatio: xy: {} {}",
                 tile.x(),
                 tile.y());
      debugPrint(log_,
                 GPL,
                 "replace",
                 5,
                 "updateInflationRatio: minxy: {} {}",
                 tile.lx(),
                 tile.ly());
      debugPrint(log_,
                 GPL,
                 "replace",
                 5,
                 "updateInflationRatio: maxxy: {} {}",
                 tile.ux(),
                 tile.uy());
      debugPrint(log_,
                 GPL,
                 "replace",
                 5,
                 "updateInflationRatio: calcInflationRatio: {}",
                 tile.inflationRatio());
    }
  }
}

// first: is Routability Need
// second: reverting procedure init need
//          (e.g. calling NesterovPlace's init())
std::pair<bool, bool> RouteBase::routability()
{
  increaseCounter();

  // create Tile Grid
  std::unique_ptr<TileGrid> tg(new TileGrid());
  tg_ = std::move(tg);
  // tg_->setLogger(log_);

  getGlobalRouterResult();

  // no need routing if RC is lower than targetRC val
  float curRc = getRC();

  if (curRc < rbVars_.targetRC) {
    resetRoutabilityResources();
    return make_pair(false, false);
  }

  //
  // saving solutions when minRc happen.
  // I hope to get lower Rc gradually as RD goes on
  //
  if (minRc_ > curRc) {
    minRc_ = curRc;
    minRcTargetDensity_ = nb_->targetDensity();
    minRcViolatedCnt_ = 0;

    // save cell size info
    for (auto& gCell : nb_->gCells()) {
      if (!gCell->isStdInstance()) {
        continue;
      }

      minRcCellSize_[&gCell - &nb_->gCells()[0]]
          = std::make_pair(gCell->dx(), gCell->dy());
    }
  } else {
    minRcViolatedCnt_++;
  }

  // set inflated ratio
  for (auto& tile : tg_->tiles()) {
    if (tile.inflationRatio() > 1) {
      tile.inflatedRatio() = tile.inflationRatio();
    } else {
      tile.inflatedRatio() = 1.0;
    }
  }

  inflatedAreaDelta_ = 0;

  // run bloating and get inflatedAreaDelta_
  for (auto& gCell : nb_->gCells()) {
    // only care about "standard cell"
    if (!gCell->isStdInstance()) {
      continue;
    }

    int idxX = (gCell->dCx() - tg_->lx()) / tg_->tileSize().x();
    int idxY = (gCell->dCy() - tg_->ly()) / tg_->tileSize().y();

    Tile& tile = tg_->tiles()[idxY * tg_->tileCnt().x() + idxX];

    // Don't care when inflRatio <= 1
    if (tile.inflatedRatio() <= 1.0) {
      continue;
    }

    int64_t prevCellArea
        = static_cast<int64_t>(gCell->dx()) * static_cast<int64_t>(gCell->dy());

    // bloat
    gCell->setSize(
        static_cast<int>(round(gCell->dx() * sqrt(tile.inflatedRatio()))),
        static_cast<int>(round(gCell->dy() * sqrt(tile.inflatedRatio()))));

    int64_t newCellArea
        = static_cast<int64_t>(gCell->dx()) * static_cast<int64_t>(gCell->dy());

    // deltaArea is equal to area * deltaRatio
    // both of original and density size will be changed
    inflatedAreaDelta_ += newCellArea - prevCellArea;

    //    inflatedAreaDelta_
    //      = static_cast<int64_t>(round(
    //        static_cast<int64_t>(gCell->dx())
    //        * static_cast<int64_t>(gCell->dy())
    //        * (tile.inflatedRatio() - 1.0)));
  }

  // target ratio
  float targetInflationDeltaAreaRatio
      = 1.0 / static_cast<float>(rbVars_.maxInflationIter);

  // TODO: will be implemented
  if (inflatedAreaDelta_
      > targetInflationDeltaAreaRatio
            * (nb_->whiteSpaceArea()
               - (nb_->nesterovInstsArea() + nb_->totalFillerArea()))) {
    // TODO dynamic inflation procedure?
  }

  log_->info(GPL, 1045, "InflatedAreaDelta: {}", inflatedAreaDelta_);
  log_->info(GPL, 1046, "TargetDensity: {}", nb_->targetDensity());

  int64_t totalGCellArea
      = inflatedAreaDelta_ + nb_->nesterovInstsArea() + nb_->totalFillerArea();

  // newly set Density
  nb_->setTargetDensity(static_cast<float>(totalGCellArea)
                        / static_cast<float>(nb_->whiteSpaceArea()));

  //
  // max density detection or,
  // rc not improvement detection -- (not improved the RC values 3 times in a
  // row)
  //
  if (nb_->targetDensity() > rbVars_.maxDensity || minRcViolatedCnt_ >= 3) {
    log_->report("Revert Routability Procedure");
    log_->info(GPL, 1047, "SavedMinRC: {}", minRc_);
    log_->info(GPL, 1048, "SavedTargetDensity: {}", minRcTargetDensity_);

    nb_->setTargetDensity(minRcTargetDensity_);

    revertGCellSizeToMinRc();

    nb_->updateDensitySize();
    resetRoutabilityResources();

    return make_pair(false, true);
  }

  log_->info(GPL, 1049, "WhiteSpaceArea: {}", nb_->whiteSpaceArea());
  log_->info(GPL, 1050, "NesterovInstsArea: {}", nb_->nesterovInstsArea());
  log_->info(GPL, 1051, "TotalFillerArea: {}", nb_->totalFillerArea());
  log_->info(GPL,
             1052,
             "TotalGCellsArea: {}",
             nb_->nesterovInstsArea() + nb_->totalFillerArea());
  log_->info(
      GPL,
      1053,
      "ExpectedTotalGCellsArea: {}",
      inflatedAreaDelta_ + nb_->nesterovInstsArea() + nb_->totalFillerArea());

  // cut filler cells accordingly
  //  if( nb_->totalFillerArea() > inflatedAreaDelta_ ) {
  //    nb_->cutFillerCells( nb_->totalFillerArea() - inflatedAreaDelta_ );
  //  }
  // routability-driven cannot solve this problem with the given density...
  // return false
  //  else {
  //    return false;
  //  }

  // updateArea
  nb_->updateAreas();

  log_->info(GPL, 1054, "NewTargetDensity: {}", nb_->targetDensity());
  log_->info(GPL, 1055, "NewWhiteSpaceArea: {}", nb_->whiteSpaceArea());
  log_->info(GPL, 1056, "MovableArea: {}", nb_->movableArea());
  log_->info(GPL, 1057, "NewNesterovInstsArea: {}", nb_->nesterovInstsArea());
  log_->info(GPL, 1058, "NewTotalFillerArea: {}", nb_->totalFillerArea());
  log_->info(GPL,
             1059,
             "NewTotalGCellsArea: {}",
             nb_->nesterovInstsArea() + nb_->totalFillerArea());

  // update densitySizes for all gCell
  nb_->updateDensitySize();

  // reset
  resetRoutabilityResources();

  return make_pair(true, true);
}

void RouteBase::revertGCellSizeToMinRc()
{
  // revert back the gcell sizes
  for (auto& gCell : nb_->gCells()) {
    if (!gCell->isStdInstance()) {
      continue;
    }

    int idx = &gCell - &nb_->gCells()[0];

    gCell->setSize(minRcCellSize_[idx].first, minRcCellSize_[idx].second);
  }
}

// extract RC values
float RouteBase::getRC() const
{
  double totalRouteOverflowH2 = 0;
  double totalRouteOverflowV2 = 0;
  int overflowTileCnt2 = 0;

  std::vector<double> horEdgeCongArray;
  std::vector<double> verEdgeCongArray;

  odb::dbGCellGrid* gGrid = db_->getChip()->getBlock()->getGCellGrid();
  for (auto& tile : tg_->tiles()) {
    for (int i = 1; i <= tg_->numRoutingLayers(); i++) {
      odb::dbTechLayer* layer = db_->getTech()->findRoutingLayer(i);
      bool isHorizontalLayer
          = (layer->getDirection() == odb::dbTechLayerDir::HORIZONTAL);

      // extract the ratio in the same way as inflation ratio cals
      float ratio
          = getUsageCapacityRatio(tile, layer, gGrid, rbVars_.ignoreEdgeRatio);

      // escape the case when blockageRatio is too huge
      if (ratio >= 0.0f) {
        if (isHorizontalLayer) {
          totalRouteOverflowH2 += fmax(0.0, -1 + ratio);
          horEdgeCongArray.push_back(ratio);
        } else {
          totalRouteOverflowV2 += fmax(0.0, -1 + ratio);
          verEdgeCongArray.push_back(ratio);
        }

        if (ratio > 1.0) {
          overflowTileCnt2++;
        }
      }
    }
  }

  log_->info(GPL, 1063, "TotalRouteOverflowH2: {}", totalRouteOverflowH2);
  log_->info(GPL, 1064, "TotalRouteOverflowV2: {}", totalRouteOverflowV2);
  log_->info(GPL, 1065, "OverflowTileCnt2: {}", overflowTileCnt2);

  int horArraySize = horEdgeCongArray.size();
  int verArraySize = verEdgeCongArray.size();

  std::sort(horEdgeCongArray.rbegin(), horEdgeCongArray.rend());
  std::sort(verEdgeCongArray.rbegin(), verEdgeCongArray.rend());

  double horAvg005RC = 0;
  double horAvg010RC = 0;
  double horAvg020RC = 0;
  double horAvg050RC = 0;
  for (int i = 0; i < horArraySize; ++i) {
    if (i < 0.005 * horArraySize) {
      horAvg005RC += horEdgeCongArray[i];
    }
    if (i < 0.01 * horArraySize) {
      horAvg010RC += horEdgeCongArray[i];
    }
    if (i < 0.02 * horArraySize) {
      horAvg020RC += horEdgeCongArray[i];
    }
    if (i < 0.05 * horArraySize) {
      horAvg050RC += horEdgeCongArray[i];
    }
  }

  horAvg005RC /= ceil(0.005 * horArraySize);
  horAvg010RC /= ceil(0.010 * horArraySize);
  horAvg020RC /= ceil(0.020 * horArraySize);
  horAvg050RC /= ceil(0.050 * horArraySize);

  double verAvg005RC = 0;
  double verAvg010RC = 0;
  double verAvg020RC = 0;
  double verAvg050RC = 0;
  for (int i = 0; i < verArraySize; ++i) {
    if (i < 0.005 * verArraySize) {
      verAvg005RC += verEdgeCongArray[i];
    }
    if (i < 0.01 * verArraySize) {
      verAvg010RC += verEdgeCongArray[i];
    }
    if (i < 0.02 * verArraySize) {
      verAvg020RC += verEdgeCongArray[i];
    }
    if (i < 0.05 * verArraySize) {
      verAvg050RC += verEdgeCongArray[i];
    }
  }
  verAvg005RC /= ceil(0.005 * verArraySize);
  verAvg010RC /= ceil(0.010 * verArraySize);
  verAvg020RC /= ceil(0.020 * verArraySize);
  verAvg050RC /= ceil(0.050 * verArraySize);

  log_->info(GPL, 1066, "0.5%RC: {}", fmax(horAvg005RC, verAvg005RC));
  log_->info(GPL, 1067, "1.0%RC: {}", fmax(horAvg010RC, verAvg010RC));
  log_->info(GPL, 1068, "2.0%RC: {}", fmax(horAvg020RC, verAvg020RC));
  log_->info(GPL, 1069, "5.0%RC: {}", fmax(horAvg050RC, verAvg050RC));

  log_->info(GPL, 1070, "0.5rcK: {}", rbVars_.rcK1);
  log_->info(GPL, 1071, "1.0rcK: {}", rbVars_.rcK2);
  log_->info(GPL, 1072, "2.0rcK: {}", rbVars_.rcK3);
  log_->info(GPL, 1073, "5.0rcK: {}", rbVars_.rcK4);

  float finalRC = (rbVars_.rcK1 * fmax(horAvg005RC, verAvg005RC)
                   + rbVars_.rcK2 * fmax(horAvg010RC, verAvg010RC)
                   + rbVars_.rcK3 * fmax(horAvg020RC, verAvg020RC)
                   + rbVars_.rcK4 * fmax(horAvg050RC, verAvg050RC))
                  / (rbVars_.rcK1 + rbVars_.rcK2 + rbVars_.rcK3 + rbVars_.rcK4);

  log_->info(GPL, 1074, "FinalRC: {}", finalRC);
  return finalRC;
}

void RouteBase::increaseCounter()
{
  numCall_++;
  inflationIterCnt_++;
  if (inflationIterCnt_ > rbVars_.maxInflationIter) {
    inflationIterCnt_ = 0;
    bloatIterCnt_++;
  }

  log_->info(GPL,
             1075,
             "Routability numCall: {} inflationIterCnt: {} bloatIterCnt: {}",
             numCall_,
             inflationIterCnt_,
             bloatIterCnt_);
}

}  // namespace gpl
