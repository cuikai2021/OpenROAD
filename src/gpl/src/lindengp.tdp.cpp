
#include "gpl/lindengp.h"

#include "db_sta/dbNetwork.hh"
#include "db_sta/dbSta.hh"
#include "placerBase.h"
#include "rsz/Resizer.hh"
#include "sta/Fuzzy.hh"
#include "sta/Units.hh"
#include "utl/metrics.hpp"

using utl::GPL;
using namespace std;

namespace gpl {

void LindenGlobalPlace::setTimingDriven(bool timing_driven)
{
  if (timing_driven) {
    timing_overflows_ = {79, 64, 49, 29, 21, 15};
  } else {
    timing_overflows_ = {};
  }
}

bool LindenGlobalPlace::updateNetWeights()
{
  update_db();
  rsz::Resizer* rs = ::ord::OpenRoad::openRoad()->getResizer();
  rs->findResizeSlacks();

  // get Top 10% worst resize nets
  sta::NetSeq& worst_slack_nets = rs->resizeWorstSlackNets();

  if (worst_slack_nets.empty()) {
    log_->warn(GPL, 1114, "No net slacks found. Timing-driven mode disabled.");
    return false;
  }

  // min/max slack for worst 10% nets
  sta::Slack slack_min = rs->resizeNetSlack(worst_slack_nets[0]);
  sta::Slack slack_max
      = rs->resizeNetSlack(worst_slack_nets[worst_slack_nets.size() - 1]);

  log_->info(GPL, 1100, "worst slack {:.3g}", slack_min);

  if (sta::fuzzyInf(slack_min)) {
    log_->warn(GPL, 1102, "No slacks found. Timing-driven mode disabled.");
    return false;
  }

  size_t weighted_net_count = 0;
  for (auto& net : pb_->nets()) {
    auto dbNet = net->dbNet();
    // default weight
    double net_weight = 1.0;
    if (net->pins().size() > 1) {
      float net_slack = rs->resizeNetSlack(dbNet);
      if (net_slack < slack_max) {
        // weight(min_slack) = net_weight_max_
        // weight(max_slack) = 1
        net_weight = 1
                     + (net_weight_max_ - 1) * (slack_max - net_slack)
                           / (slack_max - slack_min);
        weighted_net_count++;
      }
      debugPrint(log_,
                 GPL,
                 "linden-tdp",
                 3,
                 "timing: net:{} slack:{} weight:{}",
                 dbNet->getName(),
                 net_slack,
                 net_weight);
    }
    linden_db_->set_net_timing_weight(dbNet, net_weight);
  }
  log_->info(GPL, 1103, "Weighted {} nets.", weighted_net_count);
  return true;
}

void LindenGlobalPlace::report_metrics(metrics::Reporter& reporter)
{
  rsz::Resizer* rs = ::ord::OpenRoad::openRoad()->getResizer();
  rs->estimateParasitics(rsz::ParasiticsSrc::placement);
  auto sta = ::ord::OpenRoad::openRoad()->getSta();

  const int DIGITS = 2;
  auto total_negative_slack = sta->totalNegativeSlack(sta::MinMax::max());

  sta::Slack worst_slack;
  sta::Vertex* worst_vertex;
  sta->worstSlack(sta::MinMax::max(), worst_slack, worst_vertex);

  auto tns = sta->units()->timeUnit()->asString(total_negative_slack, DIGITS);
  auto wns = sta->units()->timeUnit()->asString(worst_slack, DIGITS);

  // TODO:
  // 1. shall we convert tns and wns to numbers?
  // 2. shall metrics hierarchically indexed?
  reporter["gp::tns"] = tns;
  reporter["gp::wns"] = wns;
}

}  // namespace gpl
