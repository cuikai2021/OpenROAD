#include "gpl/lindengp.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>

#include "banyan/Banyan.h"
#include "banyan_cmap.pb.h"
#include "envflag/envflag.hpp"
#include "fassert.hpp"
#include "linden/geom/dimension.hpp"
#include "linden/geom/point.hpp"
#include "linden/rdp/congestion_estimation.hpp"
#include "odb/db.h"
#include "placerBase.h"
#include "sc/ScanChain.h"
#include "utl/Logger.h"

using namespace linden::geom::dimension;

using namespace std;

using utl::GPL;

IMPORT_BOOL_FLAG_WITH_DEFAULT(LINDEN_CGR_DUMP_DEF_BEFORE_CALL_BABY_BANYAN);
IMPORT_STR_FLAG_WITH_DEFAULT(LINDEN_CGR_CMAP_DEBUGGING_FILE);
IMPORT_STR_FLAG_WITH_DEFAULT(LINDEN_TMP_FILE_ROOT_DIR);

namespace gpl {

namespace {

double cal_tile_overflow(const ::banyan::Tile& tile)
{
  if (tile.supply() == 0) {
    return 0;
  } else {
    return (tile.demand() / tile.supply()) - 1.0;
  }
}

[[maybe_unused]] void debug_print_cmap(const banyan::Cmap& cmap,
                                       const double tile_area)
{
  typedef std::tuple<int, int, char, double, double> Triple;
  std::vector<Triple> grids;
  for (int j = 0; j < cmap.hgrids(); ++j) {
    for (int i = 0; i < cmap.wgrids(); ++i) {
      const int idx = j * cmap.wgrids() + i;
      double overflows[2];
      overflows[X] = cal_tile_overflow(cmap.htiles(idx));
      overflows[Y] = cal_tile_overflow(cmap.vtiles(idx));
      for (auto dim : {X, Y}) {
        if (overflows[dim] > 0) {
          double overflow_area = overflows[dim] * tile_area;
          // push cells on Y-direction if there is congestion on X-direction,
          // vice versa.
          char c_dim = dim == X ? 'X' : 'Y';
          grids.emplace_back(
              std::make_tuple(i, j, c_dim, overflows[dim], overflow_area));
        }
      }
    }
  }
  std::sort(
      grids.begin(), grids.end(), [](const Triple& l, const Triple& r) -> bool {
        return std::get<3>(l) > std::get<3>(r);
      });
  const std::string DEBUG_PRIN_PREFIX = "// debug_print_cmap:";
  std::cout << DEBUG_PRIN_PREFIX << "cmap-overflow-grids cnt: " << grids.size()
            << std::endl;
  for (auto const& t : grids) {
    std::cout << DEBUG_PRIN_PREFIX << "(" << std::get<0>(t) << ", "
              << std::get<1>(t) << "): " << std::get<2>(t)
              << "overflow = " << std::get<3>(t)
              << ", overflow_area = " << std::get<4>(t) << std::endl;
  }
}

bool load(const string& cmap_pb_file,
          const linden::geom::Rect& core_region,
          linden::rdp::CongestionMap& cmap_result) noexcept
{
  cmap_result.setChipCoreRegion(core_region);
  banyan::Cmap cmap;
  fstream input(cmap_pb_file, ios::in | ios::binary);
  if (!input) {
    cerr << "Failed to open file:" << cmap_pb_file << endl;
    return false;
  } else if (!cmap.ParseFromIstream(&input)) {
    cerr << "Failed to parse cmap file" << cmap_pb_file << endl;
    return false;
  }
  cmap_result.initGrids(cmap.wgrids(), cmap.hgrids());
  for (int64_t j = 0; j < cmap.hgrids(); ++j) {
    for (int64_t i = 0; i < cmap.wgrids(); ++i) {
      const auto idx = j * cmap.wgrids() + i;
      for (auto dim : {X, Y}) {
        const auto& tile = dim == X ? cmap.htiles(idx) : cmap.vtiles(idx);
        cmap_result.updateSupply(dim, i, j, tile.supply());
        cmap_result.updateDemand(dim, i, j, tile.demand());
      }
    }
  }
  // FIXME: update it after expander's PR is merged
  //        and use_expander should always be FALSE in cgr mode
  const bool use_expander = false;
  if (!cmap_result.updateOverflows(use_expander)) {
    cerr << "Failed to upload overflows:" << cmap_pb_file << endl;
    return false;
  }
  return true;
}

}  // namespace

bool LindenGlobalPlace::congestion_estimate()
{
  auto sc = ::ord::OpenRoad::openRoad()->getScanChain();
  sc->reorder();
  FASSERT(routability_driven_);
  auto genCmapFileName = [] {
    const std::string basename = [] {
      auto tm = std::chrono::system_clock::now().time_since_epoch();
      auto tm_ms
          = std::chrono::duration_cast<std::chrono::milliseconds>(tm).count();
      std::ostringstream oss;
      oss << LINDEN_TMP_FILE_ROOT_DIR() << "/banyan_" << tm_ms;
      return oss.str();
    }();
    return basename + ".cmap";
  };
  std::string cmap_pb_file(LINDEN_CGR_CMAP_DEBUGGING_FILE());
  if (cmap_pb_file.empty()) {
    cmap_pb_file = genCmapFileName();
    if (LINDEN_CGR_DUMP_DEF_BEFORE_CALL_BABY_BANYAN()) {
      const std::string debug_def_file = cmap_pb_file + ".def";
      std::cout << "// banyan_debug::before_call_baby_banyan.def: "
                << debug_def_file << std::endl;
      ::ord::OpenRoad::openRoad()->writeDef(debug_def_file.c_str(), "5.8");
    }
    auto banyan = ::ord::OpenRoad::openRoad()->getBanyan();
    banyan->cmap(cmap_pb_file, cgr_mode_, "");
  }
  {
    odb::Rect coreRect;
    {
      auto odb = ord::OpenRoad::openRoad()->getDb();
      FASSERT(odb != nullptr);
      auto block = db_->getChip()->getBlock();
      FASSERT(block != nullptr);
      block->getCoreArea(coreRect);
    }
    linden::geom::Rect core_region(coreRect.ll().x(),
                                   coreRect.ll().y(),
                                   coreRect.ur().x(),
                                   coreRect.ur().y());
    FASSERT(banyan_cmap_result_);
    if (!load(cmap_pb_file, core_region, *banyan_cmap_result_)) {
      return false;
    }
  }
  return true;
}

bool LindenGlobalPlace::rdp_cmap(const double overflow)
{
  static int rdp_cnt_down = 0;
  const double OVERFLOW_THRESHOLD = 0.2;
  if (overflow > OVERFLOW_THRESHOLD) {
    return true;
  }
  if (rdp_cnt_down > 0) {
    --rdp_cnt_down;
    return true;
  }
  update_db();
  congestion_estimate();
  bool res = lindengp_->load_cmap(*banyan_cmap_result_);
  rdp_cnt_down = 10;
  return res;
}

}  // namespace gpl