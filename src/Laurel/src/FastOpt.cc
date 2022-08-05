#include "gui/gui.h"
#include "lrl/Laurel.hh"
#include "ord/OpenRoad.hh"

namespace lrl {

using namespace sta;

////////////////////////////////////////////////////////
//
//  Fast-Opt
//
////////////////////////////////////////////////////////

void Laurel::initFastOpt(OpenRoad *openroad)
{
  init(openroad,
       openroad->tclInterp(),
       openroad->getLogger(),
       // Broken gui api missing openroad accessor.
       gui::Gui::get(),
       openroad->getDb(),
       openroad->getSta(),
       openroad->getSteinerTreeBuilder(),
       openroad->getGlobalRouter());
  init();
  resizePreamble();
}

int Laurel::fastOpt()
{
  initFastOpt(openroad_);

  const int iter_limit = 5;
  VertexSet failures;
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  VertexSet* ends = sta_->search()->endpoints();

  sta_->findRequireds();
  for (Vertex* vertex : *ends) {
    Slack slack = sta_->vertexSlack(vertex, max_);
    if (!fuzzyInf(slack) && fuzzyLess(slack, slack_margin))
      failures.insert(vertex);
  }
  logger_->info(utl::LRL, 1501, "Fast-Opt found {} violations", failures.size());

  for (auto& end : failures) {
    int index = 0;
    while (quickFixSetup(end) && index < iter_limit) {
      index++;
    }
  }

  openroad_->getResizer()->estimateWireParasitics();
  logger_->info(utl::LRL,
                1500,
                "Totally resized {} cells, inserted {} buffers",
                resize_count_,
                inserted_buffer_count_);
  return inserted_buffer_count_ + resize_count_;
}

bool Laurel::quickFixSetup(Vertex /*const*/*end) 
{
  struct DrvrFanout{
    int path_id_;
    int num_fanout_;
    bool operator < (DrvrFanout const& other) const {
      return this->num_fanout_ > other.num_fanout_;
    }
  };

  PathRef worst_path = sta_->vertexWorstSlackPath(end, max_);
  PathExpanded expanded(&worst_path, sta_);
  if (expanded.size() > 1) {
    int path_length = expanded.size();
    std::vector<DrvrFanout> load_fanouts;
    int start_index = expanded.startIndex();
    for (int i = start_index; i < path_length; i++) {
      Vertex* path_vertex = pathVertex(expanded, i);
      load_fanouts.push_back({i, this->fanout(path_vertex)});
    }

    sort(load_fanouts.begin(), load_fanouts.end());

    const int& drvr_index = load_fanouts[0].path_id_;
    const int& fanout = load_fanouts[0].num_fanout_;
    PathRef *drvr_path = expanded.path(drvr_index);
    const Pin *drvr_pin = drvr_path->pin(this);
    bool tristate_drvr = isTristateDriver(drvr_pin);
    if (fanout >= split_load_min_fanout_ && !tristate_drvr){
      if (fanout < rebuffer_max_fanout_) {
        int rebuffer_count = rebuffer(drvr_pin);
        if (rebuffer_count > 0)
          return true;
      }
      splitLoads(drvr_path, drvr_index, 0.0f, &expanded);
      return true;
    }
  }
  return false;
}

}  // namespace lrl