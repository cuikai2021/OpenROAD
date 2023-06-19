#include "cgr/analyze_banyan_cmap_file.hpp"

#include <spdlog/spdlog.h>

#include <fstream>
#include <numeric>

#include "banyan_cmap.pb.h"
#include "fassert.hpp"
#include "linden/geom/dimension.hpp"
#include "linden/rdp/congestion_estimation.hpp"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "utl/metrics.hpp"

using namespace std;
using namespace linden::geom::dimension;

namespace cgr {

namespace {

auto core_region() noexcept
{
  auto coreRect = ord::OpenRoad::openRoad()->getCore();
  auto core_region = linden::geom::Rect(coreRect.ll().x(),
                                        coreRect.ll().y(),
                                        coreRect.ur().x(),
                                        coreRect.ur().y());
  return core_region;
}

struct CMapMetrics
{
  double sum;
  double max;
  double min;
  double median;
  double mean;
  size_t cnt_non_zero;
};

auto analyze_rc_values(const linden::rdp::CongestionMap& cmap,
                       metrics::Reporter& reporter) noexcept
{
  auto get_sorted_rc_vals = [&](const linden::rdp::CongestionMap& cmap) {
    std::array<std::vector<double>, 2> rc_vals_xy;
    for (auto dim : {X, Y}) {
      auto& rc_vals = rc_vals_xy[dim];
      rc_vals.reserve(cmap.wgrids() * cmap.hgrids());
      for (int64_t j = 0; j < cmap.hgrids(); ++j) {
        for (int64_t i = 0; i < cmap.wgrids(); ++i) {
          const auto rc = cmap.overflow(dim, i, j);
          FASSERT(rc >= 0)(rc);
          rc_vals.push_back(rc);
        }
      }
      std::sort(rc_vals.begin(), rc_vals.end(), greater<double>());
    }
    return rc_vals_xy;
  };
  auto rc_vals_xy = get_sorted_rc_vals(cmap);
  auto get_rc_metrics = [&](const std::vector<double>& rc_vals, size_t cnt) {
    FASSERT(cnt > 0)(cnt);
    FASSERT(cnt <= rc_vals.size())(cnt)(rc_vals.size());
    CMapMetrics rc_metrics;
    rc_metrics.sum
        = std::accumulate(rc_vals.begin(), rc_vals.begin() + cnt, 0.0);
    rc_metrics.max = rc_vals.front();
    rc_metrics.min = *(rc_vals.begin() + (cnt - 1));
    rc_metrics.median = rc_vals[cnt / 2];
    rc_metrics.mean = rc_metrics.sum / cnt;
    rc_metrics.cnt_non_zero = std::count_if(
        rc_vals.begin(), rc_vals.begin() + cnt, [](auto const& v) {
          return v > 0;
        });
    return rc_metrics;
  };
  const std::array<double, 5> PERCENTILES = {0.5, 1.0, 2.0, 5.0, 100.0};
  const std::array<string, 5> PERCENTILE_NAMES
      = {"5‰RC", "10‰RC", "20‰RC", "50‰RC", "AllRC"};
  for (auto dim : {X, Y}) {
    const auto& rc_vals = rc_vals_xy[dim];
    for (size_t i = 0; i < PERCENTILES.size(); ++i) {
      size_t percentile_sz = rc_vals.size() * (PERCENTILES[i] / 100.0);
      if (percentile_sz == 0) {
        continue;
      }
      const auto rc_metrics = get_rc_metrics(rc_vals, percentile_sz);
      auto& r = reporter["rdp::banyan_cmap"][PERCENTILE_NAMES[i]]
                        [dim == X ? "X" : "Y"];
      r["sum"] = rc_metrics.sum;
      r["max"] = rc_metrics.max;
      r["min"] = rc_metrics.min;
      r["median"] = rc_metrics.median;
      r["mean"] = rc_metrics.mean;
      r["cnt_non_zero"] = rc_metrics.cnt_non_zero;
    }
  }
  const auto congestion = cmap.getCurRouteCongestionValue();
  reporter["rdp::banyan_cmap"]["FinalRC"] = congestion;
}

}  // namespace

bool load_cmap(const string& cmap_pb_file,
               linden::rdp::CongestionMap& cmap_result) noexcept
{
  cmap_result.setChipCoreRegion(core_region());
  banyan::Cmap cmap;
  fstream input(cmap_pb_file, ios::in | ios::binary);
  if (!input) {
    spdlog::error("Failed to open file: {}", cmap_pb_file);
    return false;
  } else if (!cmap.ParseFromIstream(&input)) {
    spdlog::error("Failed to parse cmap file: {}", cmap_pb_file);
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
    spdlog::error("Failed to update overflows: {}", cmap_pb_file);
    return false;
  }
  return true;
}

void analyze_banyan_cmap_file(const char* cmap_file,
                              const char* report_file) noexcept
{
  if (cmap_file == nullptr || string(cmap_file).empty()) {
    spdlog::error(
        "missing the cmap pb file, pls specify it with -cmap_pb_file");
    return;
  }
  string cmap_pb_file(cmap_file);
  linden::rdp::CongestionMap banyan_cmap_result;
  spdlog::info("loading banyan cmap file: {}", cmap_pb_file);
  if (!load_cmap(cmap_pb_file, banyan_cmap_result)) {
    return;
  }
  spdlog::info("end loading banyan cmap file: {}", cmap_pb_file);
  metrics::Config mcfg = metrics::Config::sink_to_stderr();
  if (report_file == nullptr || string(report_file).empty()) {
    spdlog::info(
        "writing report to stderr. YOu can specify it with -report_json_file");
  } else {
    string_view filename = report_file;
    FASSERT(!filename.empty());
    spdlog::info("writing report to {}", filename);
    mcfg = metrics::Config::sink_to_file(filename);
  }
  metrics::Reporter reporter(mcfg);
  analyze_rc_values(banyan_cmap_result, reporter);
}

}  // namespace cgr