#include "linden/analyze_banyan_cmap_file.hpp"
#include "../util/odb_util.hpp"
#include "linden/geom/dimension.hpp"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "utl/metrics.hpp"
#include "banyan_cmap.pb.h"
#include "fassert.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>
#include <string_view>
#include <tuple>
#include <numeric>

using namespace std;
using namespace linden::geom::dimension;

namespace linden::cgr {

namespace {

double calc_fulfill(double demand, double supply) noexcept {
    if (supply == 0) {
        return 100; // an arbitrary but large enough value
    }
    return demand / supply;
}

void analyze_rc_values(
    const banyan::Cmap& cmap,
    metrics::Reporter& reporter) noexcept
{
    std::array<std::vector<double>, 2> rc_vals_xy;
    {
        for (auto dim : {X, Y}) {
            auto& rc_vals = rc_vals_xy[dim];
            rc_vals.reserve(cmap.wgrids() * cmap.hgrids());
            auto const& tiles = (dim == X ? cmap.htiles() : cmap.vtiles());
            for (auto const& tile: tiles) {
                rc_vals.push_back(calc_fulfill(tile.demand(), tile.supply()));
            }
            std::sort(rc_vals.begin(), rc_vals.end(), greater<double>());
        }
    }
    FASSERT(rc_vals_xy[X].size() == rc_vals_xy[Y].size())
        (rc_vals_xy[X].size())
        (rc_vals_xy[Y].size());
    size_t n = rc_vals_xy[X].size();

    std::array<tuple<double, string_view>, 4> PERCENTILES {{
        {0.05, "top-5%"},
        {0.1, "top-10%"},
        {0.2, "top-20%"},
        {0.5, "top-50%"},
    }};
    auto& s = reporter["banyan_cmap"];
    for (auto dim: {X, Y}) {
        auto& r = s[dim == X ? "X" : "Y"];

        const auto& rc_vals = rc_vals_xy[dim];
        r["size"] = n;

        auto sum = std::accumulate(rc_vals.begin(), rc_vals.end(), 0.0);
        r["sum"] = sum;
        r["max"] = rc_vals.front();
        r["min"] = rc_vals.back();
        r["mean"] = sum / n;
        r["cnt_non_zero"] = count_if(
            rc_vals.begin(),
            rc_vals.end(),
            [](auto v) {return v > 0;});
        for (auto const& [pc, key]: PERCENTILES) {
            r[key] = rc_vals[n * pc];
        }
    }
    {
        std::array<std::vector<double>, 2> accumulated_fulfill_xy;
        for (auto dim: {X, Y}) {
            const auto& rc_vals = rc_vals_xy[dim];
            auto& accumulated_fulfill = accumulated_fulfill_xy[dim];
            accumulated_fulfill.reserve(rc_vals.size());

            double n = 0;
            double sum = 0;
            for(auto x: rc_vals) {
                sum += x;
                n += 1;
                accumulated_fulfill.push_back(sum / n);
            }
        }
        const std::array<tuple<double, double>, 2> SAMPLE_COEFFS {{
            {0.005, 1.0},
            {0.01, 1.0},
        }};
        double sum = 0;
        double coeff_sum = 0;
        for (const auto& [sample, coeff]: SAMPLE_COEFFS) {
            size_t idx = n * sample;
            double v = std::max(
                accumulated_fulfill_xy[X][idx],
                accumulated_fulfill_xy[Y][idx]);
            sum += v * coeff;
            coeff_sum += coeff;
        }
        s["one_number"] = sum / coeff_sum;
    }
}

}  // namespace


bool analyze_banyan_cmap_file(
    const filesystem::path& cmap_file,
    const filesystem::path& report_file) noexcept
{
    spdlog::info("load banyan cmap file: {}", cmap_file);
    banyan::Cmap cmap;
    ifstream input(cmap_file, ios::binary);
    if (!input) {
        spdlog::error("Failed to open file: {}", cmap_file);
        return false;
    } else if (!cmap.ParseFromIstream(&input)) {
        spdlog::error("Failed to parse cmap file: {}", cmap_file);
        return false;
    }

    metrics::Config mcfg = metrics::Config::sink_to_stderr();
    if (report_file.empty()) {
        spdlog::info(
            "writing report to stderr. "
            "If you want to write it to a file, please specify `-report_json_file`.");
    } else {
        spdlog::info("writing report to {}", report_file);
        mcfg = metrics::Config::sink_to_file(report_file);
    }
    metrics::Reporter reporter(mcfg);
    analyze_rc_values(cmap, reporter);
    return true;
}

}  // namespace linden::cgr