#pragma once

#include <vector>

namespace linden {
namespace HpwlConfig {
// F1 Moving average window length
static constexpr int64_t MOVING_AVG_LEN = 10;
// F1 Peak equivalent overflow, used to enable rebound detection
static constexpr double PEEK_OVERFLOW = 0.5;
// F1 Curve rebound ratio, used to determine repulsion dominance
static constexpr double REBOUND_RATIO = 1.2;

// F1 Heat Death detection window count
static constexpr int64_t HEAT_DEATH_NUM = 10;
static constexpr int64_t HEAT_DEATH_LEN = 20;
static constexpr double HEAD_DEATH_RATIO = 0.02;

// F1 Temporary storage
static constexpr int64_t LAG_STEP = 50;

} // namespace HpwlConfig

struct HpwlAnalyze
{
    HpwlAnalyze();

    void push(int64_t hpwl, double overflow);
    bool repulsion_dominance();
    bool heat_death();
    bool best_gpl_postion();
    void save_data();

    double _gpl_best_postion;
    double _min_moving_avg;
    double _min_lagged_ratio;
    int64_t _count;
    std::vector<int64_t> _hpwl_vec;
    std::vector<double> _overflow_vec;
    std::vector<double> _f1_vec;
    std::vector<double> _accumulated_f1;
};

} // namespace linden