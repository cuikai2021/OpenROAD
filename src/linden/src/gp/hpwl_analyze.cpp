#include "hpwl_analyze.hpp"

#include <spdlog/spdlog.h>

#include <limits>

namespace linden {

HpwlAnalyze::HpwlAnalyze()
: _gpl_best_postion(std::numeric_limits<double>::infinity()),
  _min_moving_avg(std::numeric_limits<double>::infinity()),
  _min_lagged_ratio(std::numeric_limits<double>::infinity()),
  _count(0)
{
    spdlog::info("[iPlace/ha] MOVING_AVG_LEN: {}", HpwlConfig::MOVING_AVG_LEN);
    spdlog::info("[iPlace/ha] PEEK_OVERFLOW: {:.2f}", HpwlConfig::PEEK_OVERFLOW);
    spdlog::info("[iPlace/ha] REBOUND_RATIO: {:.2f}", HpwlConfig::REBOUND_RATIO);
    spdlog::info("[iPlace/ha] HEAT_DEATH_NUM: {}", HpwlConfig::HEAT_DEATH_NUM);
    spdlog::info("[iPlace/ha] HEAT_DEATH_LEN: {}", HpwlConfig::HEAT_DEATH_LEN);
    spdlog::info("[iPlace/ha] HEAD_DEATH_RATIO: {:.2f}", HpwlConfig::HEAD_DEATH_RATIO);
    spdlog::info("[iPlace/ha] LAG_STEP: {}", HpwlConfig::LAG_STEP);

    // Initialize accumulated F1
    _accumulated_f1.push_back(0);
}

void HpwlAnalyze::push(int64_t hpwl, double overflow)
{
    // Use HpwlConfig from the global struct
    _hpwl_vec.push_back(hpwl);
    _overflow_vec.push_back(overflow);

    // F1
    const double f1 = hpwl * overflow;
    _f1_vec.push_back(f1);
    _accumulated_f1.push_back(_accumulated_f1.back() + f1);

    ++_count;
}

bool HpwlAnalyze::repulsion_dominance()
{
    // Tip 1: Limit the minimum point value range
    if (_overflow_vec.empty() || _overflow_vec.back() > HpwlConfig::PEEK_OVERFLOW) {
        return false;
    }
    double moving_avg = _accumulated_f1.back() - _accumulated_f1[_count - 1 - HpwlConfig::MOVING_AVG_LEN];
    _min_moving_avg = std::min(_min_moving_avg, moving_avg);

    if (_min_moving_avg * HpwlConfig::REBOUND_RATIO < moving_avg) {
        spdlog::info(
            "[iPlace/ha] Repulsion Dominance:"
            "\n  iter = {}"
            "\n  min_moving_avg_ = {}"
            "\n  moving_avg = {}",
            _count,
            _min_moving_avg,
            moving_avg);
        return true;
    }

    return false;
}

bool HpwlAnalyze::heat_death()
{
    if (_overflow_vec.empty() || _overflow_vec.back() > HpwlConfig::PEEK_OVERFLOW) {
        return false;
    }

    if (_count % 100 == 0 && 
        _count > HpwlConfig::HEAT_DEATH_LEN * HpwlConfig::HEAT_DEATH_NUM) 
    {
        double min_sum = std::numeric_limits<double>::infinity();
        double max_sum = -std::numeric_limits<double>::infinity();

        for (size_t i = 0; i < HpwlConfig::HEAT_DEATH_NUM; i++) {
            const double last = _count - 1 - i * HpwlConfig::HEAT_DEATH_LEN;
            const double sum = _accumulated_f1[last] - _accumulated_f1[last - HpwlConfig::HEAT_DEATH_LEN];

            min_sum = std::min(min_sum, sum);
            max_sum = std::max(max_sum, sum);
        }

        const double volatility = (max_sum - min_sum) / min_sum;
        if (volatility < HpwlConfig::HEAD_DEATH_RATIO) {
            spdlog::info(
                "[iPlace/ha] Heat Death:"
                "\n  iter = {}"
                "\n  volatility = {}"
                "\n  max_sum = {}"
                "\n  min_sum = {}",
                _count,
                volatility,
                max_sum,
                min_sum);
            return true;
        }
    }

    return false;
}

bool HpwlAnalyze::best_gpl_postion()
{
    if (_overflow_vec.empty() || _overflow_vec.back() > HpwlConfig::PEEK_OVERFLOW || _count <= HpwlConfig::LAG_STEP) {
        return false;
    }
    // Min Lagged Ratio
    const double lagged_ratio =
        _f1_vec.back() / _f1_vec[_count - 1 - HpwlConfig::LAG_STEP];
    if (_count % 5 == 0 && _min_lagged_ratio > lagged_ratio) {
        _min_lagged_ratio = lagged_ratio;
        return true;
    }

    return false;
}

} // namespace linden