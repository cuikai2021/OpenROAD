#include "lrl/SetupFixer.hh"

namespace lrl {

using namespace sta;

////////////////////////////////////////////////////////
//
//  class ClockConcurrentFixer
//
////////////////////////////////////////////////////////

void ClockConcurrentFixer::initAlgorithm()
{
  VertexSet* ends = sta_->search()->endpoints();
  VertexSet failures;
  for (Vertex* vertex : *ends) {
    Slack slack = sta_->vertexSlack(vertex, MinMax::max());
    if (!fuzzyInf(slack))
      failures.insert(vertex);
  }
  logger_->info(utl::LRL, 1600, "CCOPT detected {} setup violations", failures.size());
  buildIndexMap(failures);
}

void ClockConcurrentFixer::addConstraintPrimeInOut(Vertex* in, Vertex* out)
{
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  Index o_index = index_graph_[out];
  Index i_index = index_graph_[in];

  PathRef path = sta_->vertexWorstSlackPath(out, max_);
  Arrival arrival = path.vertex(sta_)
                    ? path.arrival(sta_)
                    : getArrival(out, max_);
  Required req = sta_->vertexRequired(in, MinMax::max()) + slack_margin;

  Delay zero_skew = arrival - req;
  solver_.addConstraint({i_index, o_index, MCFSolver::inf, -delay2Cost(zero_skew)});
  opt_regs_.insert(in, out, zero_skew);
  debugPrint(logger_,
          utl::LRL,
          "LRL",
          4,
          "reg {} -> {} : cost = {}",
          in->name(sta_->sdcNetwork()),
          out->name(sta_->sdcNetwork()),
          delayAsString(zero_skew, sta_, 3));
}

void ClockConcurrentFixer::addConstraintPrimeOut(Vertex* po)
{
  bool t = false;
  if (!sta_->network()->isTopLevelPort(po->pin())
      && opt_regs_.find(po, t) != NULL)
    return;
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  Index po_index = index_graph_[po];
  Required req = sta_->vertexRequired(po, MinMax::max()) + slack_margin;
  solver_.addConstraintSnk(po_index, delay2Cost(req));
  bellman_max_iter_num_ = index_graph_.size();
}

#define INST_NAME(pin) \
  (sta_->network()->pathName(sta_->network()->instance(pin)))

void ClockConcurrentFixer::addConstraintPrimeIn(Vertex* pi)
{
  Pin const* pi_pin = pi->pin();
  if (!sta_->network()->isTopLevelPort(pi_pin)) {
    const char *inst_name = INST_NAME(pi_pin);
    VertexSet* ends = sta_->search()->endpoints();
    bool found = false;
    for (Vertex* end : *ends) {
      Pin const* end_pin = end->pin();
      if (!sta_->network()->isTopLevelPort(end_pin)) {
        const char *end_inst_name = INST_NAME(end_pin);
        if (strcmp(inst_name, end_inst_name) == 0
            && !sta_->isClock(end_pin)
            && !fuzzyInf(sta_->vertexSlack(end, max_))) {
          addConstraintPrimeInOut(end, pi);
          found = true;
        }
      }
    }
    if (found)
      return;
  }
  Index pi_index = index_graph_[pi];
  PathRef path = sta_->vertexWorstSlackPath(pi, max_);
  Arrival pi_arrival = path.vertex(sta_) ? 
                      path.arrival(sta_) :
                      getArrival(pi, max_);
  solver_.addConstraintSrc(pi_index, -delay2Cost(pi_arrival));
}

Delay ClockConcurrentFixer::getUsefulSkew(Vertex const* vertex)
{
  Delay new_skew = LRL_H_ZERO;
  bool is_i2o = false;
  Reg2RegSkewMap::NodeSkewMap* m = opt_regs_.find((Vertex*)vertex, is_i2o);
  if (!m) return LRL_H_ZERO;
  for (auto const & e : *m) {
    Delay origin_skew = e.second;
    Vertex* vi =  is_i2o ? (Vertex*)vertex : e.first;
    Vertex* vo = !is_i2o ? (Vertex*)vertex : e.first;
    Delay skew = index_graph_.result(vi) 
                  - index_graph_.result(vo)
                  - origin_skew;
    if (fuzzyLess(LRL_H_ROUNDING_LIMIT, abs(skew))
        && fuzzyLess(std::abs(new_skew), std::abs(skew)))
      new_skew = skew;
  }
  return new_skew;
}

void ClockConcurrentFixer::report_skews()
{
  // for (auto& input : reg_outs_)
  //   printf("reg %s, skew = %s\n",
  //          input->name(sta_->sdcNetwork()),
  //          delayAsString(getUsefulSkew(input), sta_, 3));
}

}  // namespace lrl