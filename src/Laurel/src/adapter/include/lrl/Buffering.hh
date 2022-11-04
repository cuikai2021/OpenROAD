#pragma once

#include <deque>

#include "lrl/LaurelAdapter.hh"

namespace lrl {

using namespace sta;

// interface
struct HoldBufferingSource
{
  HoldBufferingSource() = default;
  HoldBufferingSource(HoldBufferingSource const& s) = delete;
  void operator=(HoldBufferingSource const&) = delete;
  
  // get insert delay padding for a specified wire
  virtual Delay getWirePadding(Wire const& w) const = 0;

  // get all fanout wire driven by a specified vertex
  virtual void getAllWireFrom(Vertex* v, WireSet& wire_set) = 0;

  // get all driver vertex (pred) which needs to insert buffers
  virtual void getNonZeroPaddingPred(VertexSet& vio_pred_set) = 0;
};

class HoldBuffering : public LaurelBase
{
 public:
  HoldBuffering(OpenRoad* openroad, Options* options)
    : LaurelBase(openroad, options) { sortHoldBuffers(); };

  bool insertAllBuffer(HoldBufferingSource* source);

  int getInsertedBufferCnt() const { return inserted_buffer_count_; }

 private:
  struct BufferDelay
  {
    LibertyCell* buffer;
    Delay hold_delay;
    bool operator<(BufferDelay const& other) const
    {
      return this->hold_delay > other.hold_delay;
    }
  };
  typedef std::deque<BufferDelay> BufferSeq;
  void sortHoldBuffers();

  // insert buffers
  Pin* findFanoutPin(Vertex* pred, Vertex* succ) const;
  Vertex* insertHoldDelay(Vertex* drvr,
                          int buffer_count,
                          PinSeq& load_pins,
                          bool loads_have_out_port,
                          LibertyCell* buffer_cell);
  LibertyCell* findHoldBuffer(Delay min_limit, int& count, Delay& real_delay);
  bool insertBuffer(Vertex* insert_from,
                    Delay decrease,
                    WireSet& drvr_pair_set,
                    HoldBufferingSource* source,
                    Vertex* drvr = nullptr);
  // variables
  BufferSeq hold_buffers_;
};

}  // namespace lrl