#include "gui/gui.h"
#include "lrl/Laurel.hh"
#include "ord/OpenRoad.hh"
#include "sta/NetworkClass.hh"

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
  parasiticPreamble(openroad_->getResizer());
}

int Laurel::fastOpt(int iter)
{
  const Slack slack_margin = options_->slackMargin() / LRL_H_STA_SCALING;
  const Delay uncertainty = options_->uncertainty() / LRL_H_STA_SCALING;

  initFastOpt(openroad_);
  incrementalParasiticsBegin();
  for (int i = 0; i<iter; ++i) {
    sta_->findRequireds();
    VertexIterator vertex_iter(sta_->graph());
    VertexSet failures;
    bool changed = false;
    while (vertex_iter.hasNext()) {
      Vertex* v = vertex_iter.next();
      Pin* pin = v->pin();
      if(network_->isDriver(pin) 
        && !network_->isTopLevelPort(pin)) {
        Slack slack = sta_->vertexSlack(v, max_);
        if ( this->fanout(v) > 4
          && !fuzzyInf(slack)
          && fuzzyLess(slack, slack_margin + uncertainty))
          failures.insert(v);
      }
    }
    logger_->info(utl::LRL, 1501, "Fast-Opt found {} violated high fanout net, iter {}", failures.size(), i+1);
    if (failures.empty())
      break;

    for (auto& v : failures) {
      if (quickFixSetup(v))
        changed = true;
      updateParasitics();
    }
    if (!changed)
      break;
  }
  incrementalParasiticsEnd();
  logger_->info(utl::LRL,
                1500,
                "Totally resized {} cells, inserted {} buffers",
                resize_count_,
                inserted_buffer_count_);
  return inserted_buffer_count_ + resize_count_;
}

bool Laurel::quickFixSetup(Vertex* v)
{
  Pin *pin = v->pin();
  PathRef path = sta_->vertexWorstSlackPath(v, max_);
  if(!path.vertex(sta_))
    return false;
  const int fanout = this->fanout(v);
  if (!isTristateDriver(pin)) {
    if (fanout < rebuffer_max_fanout_) {
      int rebuffer_count = rebuffer(pin);
      if (rebuffer_count > 0)
        return true;
    }
    if (fanout > split_load_min_fanout_) {
      splitFanoutLoads(&path);
      return true;
    }
  }
  return false;
}

void Laurel::splitFanoutLoads(PathRef *drvr_path)
{
  struct FanoutSlack
  {
    Vertex* fanout;
    Slack slack;
    bool operator<(FanoutSlack const& other) const
    {
      return this->slack > other.slack;
    }
  };

  Pin* drvr_pin = drvr_path->pin(this);
  RiseFall const* rf = drvr_path->transition(sta_);
  std::vector<FanoutSlack> fv;
  VertexOutEdgeIterator edge_iter(drvr_path->vertex(sta_), graph_);
  while (edge_iter.hasNext()) {
    Vertex* v = edge_iter.next()->to(graph_);
    Slack s = sta_->vertexSlack(v, rf, max_);
    fv.push_back({v, s});
  }
  sort(fv.begin(), fv.end());

  Instance* buffer;
  LibertyCell* buffer_cell = buffer_lowest_drive_;
  Point loc = db_network_->location(drvr_pin);
  makeBufferInstance("split", loc, buffer_cell, buffer);

  Net* net = network_->net(drvr_pin);
  Net* out_net = makeUniqueNet();
  LibertyPort *input, *output;
  buffer_cell->bufferPorts(input, output);
  sta_->connectPin(buffer, input, net);
  parasiticsInvalid(net);
  sta_->connectPin(buffer, output, out_net);

  int split_index = fv.size() / 2;
  for (int i = 0; i < split_index; i++) {
    Pin *load_pin = fv[i].fanout->pin();
    if (!network_->isTopLevelPort(load_pin)) {
      connectNetLoad(load_pin, out_net);
    }
  }
  Pin* buffer_out_pin = network_->findPin(buffer, output);
  resizeToTargetSlew(buffer_out_pin, false);
}

////////////////////////////////////////////////////////
//
//  class LaurelAdapter
//
////////////////////////////////////////////////////////

void LaurelAdapter::makeBufferInstance(const char* tag,
                                       Point const& location,
                                       LibertyCell* buffer_cell,
                                       Instance*& buffer)
{
  string buffer_name = makeUniqueInstName(tag);
  Instance* parent = db_network_->topInstance();
  buffer = makeInstance(buffer_cell,
                        buffer_name.c_str(),
                        parent);
  
  journalMakeBuffer(buffer);
  inserted_buffer_count_++;
  designAreaIncr(area(db_network_->cell(buffer_cell)));
  setLocation(buffer, location);
}

void LaurelAdapter::connectNetLoad(Pin* pin, Net* net)
{
  LibertyPort* port = network_->libertyPort(pin);
  Instance* inst = network_->instance(pin);
  sta_->disconnectPin(pin);
  sta_->connectPin(inst, port, net);
}

}  // namespace lrl