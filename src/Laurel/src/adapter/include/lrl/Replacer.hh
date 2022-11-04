#pragma once

#include "lrl/LaurelAdapter.hh"

namespace lrl {

using namespace sta;

typedef float Resistance;
typedef float Capacitance;

struct DelayFactor
{
  Delay delay = 0.0f;
  Resistance prev_drive = 0.0f;
  LibertyPort* in_port = nullptr;
  DcalcAnalysisPt* dcalc_ap = nullptr;
};

struct ResizeInfo
{
  DelayFactor* info = nullptr;
  Delay new_delay = INF;
  void store(ResizeInfo const& i)
  {
    if (fuzzyLess(i.new_delay, new_delay))
      *this = i;
  }
};
typedef CompareMap<Vertex*, ResizeInfo> ResizeMap;

// interface
struct SetupReplacerSource
{
  SetupReplacerSource() = default;
  SetupReplacerSource(SetupReplacerSource const& s) = delete;
  void operator=(SetupReplacerSource const&) = delete;

  // get all resize vertex
  virtual void getResizeVertex(ResizeMap& resize_vertex) = 0;
};

class SetupReplacer : public LaurelBase
{
 public:
  SetupReplacer(OpenRoad* openroad, Options* options)
      : LaurelBase(openroad, options) { resizePreamble(); };

  bool resizeAllCells(SetupReplacerSource* source);
  int getResizeCnt() const { return resize_count_; }

 protected:
  LibertyCell* findResizeCell(Vertex* v, ResizeInfo const& i);
  void getResizeQueue(sta::Map<Vertex*, LibertyCell*>& queue,
                      SetupReplacerSource* source);
};

}  // namespace lrl
