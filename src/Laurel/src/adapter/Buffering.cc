#include "lrl/Buffering.hh"

namespace lrl {

using namespace sta;

////////////////////////////////////////////////////////
//
//  class HoldBuffering
//
////////////////////////////////////////////////////////

void HoldBuffering::sortHoldBuffers()
{
  findBuffers();
  for (LibertyCell* buffer : buffer_cells_)
    hold_buffers_.push_back({buffer, bufferHoldDelay(buffer)});

  sort(hold_buffers_.begin(), hold_buffers_.end());
}

LibertyCell* HoldBuffering::findHoldBuffer(Delay min_limit,
                                           int& count,
                                           Delay& real_delay)
{
  Delay const uncertainty = options_->uncertainty() / LRL_H_STA_SCALING;
  Delay const upper_limit = min_limit + uncertainty;
  Delay const& min_delay = hold_buffers_.back().hold_delay;
  LibertyCell* max_buffer = hold_buffers_.back().buffer;
  Delay max_delay = min_delay;
  count = 1;
  real_delay = max_delay;

  if (!fuzzyLess(min_limit, min_delay)) {
    for (auto& h : hold_buffers_) {
      Delay const& d = h.hold_delay;
      int min_count = std::ceil(min_limit / d);
      Delay fixed_delay = d * (Delay) (min_count);
      if (min_count > 1 && fuzzyLess(upper_limit, fixed_delay))
        fixed_delay = d * (Delay) (--min_count);

      if (fuzzyLess(fixed_delay + min_delay, upper_limit)
          || fuzzyLess(std::abs(fixed_delay - min_limit), uncertainty)) {
        max_buffer = h.buffer;
        max_delay = d;
        real_delay = fixed_delay;
        count = min_count;
        break;
      }
    }
  }

  if (max_buffer)
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               4,
               "choose buffer {}, delay {}, fixed delay {}",
               max_buffer->name(),
               delayAsString(max_delay, this, 3),
               delayAsString(real_delay, this, 3));
  return max_buffer;
}

#define PIN_NET(pin)                                      \
  (sta_->network()->isTopLevelPort(pin)                   \
       ? sta_->network()->net(sta_->network()->term(pin)) \
       : sta_->network()->net(pin))

Vertex* HoldBuffering::insertHoldDelay(Vertex* drvr,
                                       int buffer_count,
                                       PinSeq& load_pins,
                                       bool loads_have_out_port,
                                       LibertyCell* buffer_cell)
{
  Pin* drvr_pin = drvr->pin();
  Net* drvr_net = PIN_NET(drvr_pin);
  Net *in_net, *out_net;
  if (loads_have_out_port) {
    // Verilog uses nets as ports, so the net connected to an output port has
    // to be preserved.
    // Move the driver pin over to gensym'd net.
    in_net = makeUniqueNet();
    connectNetLoad(drvr_pin, in_net);
    out_net = drvr_net;
  } else {
    in_net = drvr_net;
    out_net = makeUniqueNet();
  }
  parasiticsInvalid(in_net);

  Point drvr_loc = sta_->getDbNetwork()->location(drvr_pin);
  Point load_center = findCenter(load_pins);
  int dx = (load_center.x() - drvr_loc.x()) / (buffer_count + 1);
  int dy = (load_center.y() - drvr_loc.y()) / (buffer_count + 1);

  Net* buf_in_net = in_net;
  Instance* buffer = nullptr;
  LibertyPort *input, *output;
  buffer_cell->bufferPorts(input, output);
  for (int i = 0; i < buffer_count; i++) {
    Net* buf_out_net = (i == buffer_count - 1) ? out_net : makeUniqueNet();
    Point buffer_loc(drvr_loc.x() + dx * i, drvr_loc.y() + dy * i);
    makeBufferInstance("hold", buffer_loc, buffer_cell, buffer);

    sta_->connectPin(buffer, input, buf_in_net);
    sta_->connectPin(buffer, output, buf_out_net);
    buf_in_net = buf_out_net;
    parasiticsInvalid(buf_out_net);
  }

  for (Pin* load_pin : load_pins) {
    Net* load_net = PIN_NET(load_pin);
    if (load_net != out_net) {
      connectNetLoad(load_pin, out_net);
    }
  }

  Pin* out_pin = db_network_->findPin(buffer, output);
  return sta_->graph()->pinLoadVertex(out_pin);
}

Pin* HoldBuffering::findFanoutPin(Vertex* pred, Vertex* succ) const
{
  if (!succ->isDriver(sta_->network()))
    return succ->pin();
  VertexOutEdgeIterator pred_edge_iter(pred, sta_->graph());
  while (pred_edge_iter.hasNext()) {
    Edge* pred_edge = pred_edge_iter.next();
    Vertex* fanout = pred_edge->to(sta_->graph());
    VertexInEdgeIterator succ_edge_iter(succ, sta_->graph());
    while (succ_edge_iter.hasNext()) {
      Edge* succ_edge = succ_edge_iter.next();
      Vertex* fanin = succ_edge->from(sta_->graph());
      if (fanin == fanout)
        return fanin->pin();
    }
  }
  return nullptr;
}

bool HoldBuffering::insertBuffer(Vertex* insert_from,
                                 Delay decrease,
                                 WireSet& drvr_pair_set,
                                 HoldBufferingSource* source,
                                 Vertex* drvr)
{
  PinSeq load_pins;
  bool loads_have_out_port = false;
  Delay max_padding = LRL_H_ZERO;
  Delay min_padding = INF;
  if (drvr == nullptr)
    drvr = insert_from;
  if (drvr_pair_set.empty())
    source->getAllWireFrom(insert_from, drvr_pair_set);

  for (auto& wire_pair : drvr_pair_set) {
    Delay padding = source->getWirePadding(wire_pair) - decrease;
    if (fuzzyLess(LRL_H_ROUNDING_LIMIT, padding)) {
      Vertex* fanout = wire_pair.second;
      Pin* fanout_pin = findFanoutPin(drvr, fanout);
      if (fanout_pin != nullptr) {
        load_pins.push_back(fanout_pin);
        if (sta_->network()->direction(fanout_pin)->isAnyOutput()
            && sta_->network()->isTopLevelPort(fanout_pin))
          loads_have_out_port = true;

        if (fuzzyLess(max_padding, padding))
          max_padding = padding;
        if (fuzzyLess(padding, min_padding) || fuzzyInf(min_padding))
          min_padding = padding;
      }
    }
  }

  const Delay uncertainty = options_->uncertainty() / LRL_H_STA_SCALING;
  Delay padding = max_padding;
  if (!fuzzyLess(max_padding - min_padding, uncertainty))
    padding = min_padding;

  int buffer_count = 1;
  Delay insert_delay = LRL_H_ZERO;
  LibertyCell* buffer_cell
      = findHoldBuffer(padding, buffer_count, insert_delay);
  if (buffer_cell != nullptr && !load_pins.empty()) {
    Vertex* buffer_fanout = insertHoldDelay(
        drvr, buffer_count, load_pins, loads_have_out_port, buffer_cell);
    debugPrint(logger_,
               utl::LRL,
               "LRL",
               4,
               "inserted {} buffer, buffer fanout vertex = {}, decrease = {}, "
               "max_padding = {}",
               std::to_string(buffer_count),
               buffer_fanout->name(sta_->sdcNetwork()),
               delayAsString(insert_delay, this, 3),
               delayAsString(max_padding, this, 3));
    if (fuzzyLessEqual(insert_delay, max_padding))
      return insertBuffer(insert_from,
                          decrease + insert_delay,
                          drvr_pair_set,
                          source,
                          buffer_fanout);
    return true;
  }
  return false;
}

bool HoldBuffering::insertAllBuffer(HoldBufferingSource* source)
{
  inserted_buffer_count_ = 0;
  bool ret = true;
  VertexSet vio_pred_set;
  source->getNonZeroPaddingPred(vio_pred_set);
  for (auto& v : vio_pred_set) {
    WireSet drvr_pair_set;
    if (!insertBuffer(v, LRL_H_ZERO, drvr_pair_set, source))
      ret = false;
  }
  debugPrint(logger_,
             utl::LRL,
             "LRL",
             1,
             "inserted {} buffers",
             inserted_buffer_count_);
  return ret;
}

}  // namespace lrl
