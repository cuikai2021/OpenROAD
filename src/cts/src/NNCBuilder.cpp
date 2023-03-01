#include "NNCBuilder.h"

#include <fstream>
#include <iostream>
#include <cmath>
#include <map>
#include <queue>

#include "utl/Logger.h"
#include "lrl/DelayCalc.hh"
#include "sta/Liberty.hh"

namespace cts {

using utl::CTS;
using utl::Logger;

void NNCBuilder::run() {
  logger_->info(CTS, 201, "Generating NCC topology for net {}.", _clock.getName());
  logger_->info(CTS, 202, " Total number of sinks: {}.", _clock.getNumSinks());

  //get the delay&cap of the root of the subtrees
  std::map<std::string, std::pair<float, float>> cell_cap_delay_map;
  if (this->getChildren().size() > 0) {
    for (auto child : this->getChildren()) {
      NNCBuilder *builder = dynamic_cast<NNCBuilder*>(child);
      odb::dbObject* driverPin = builder->getClock().getDriverPin();
      if (driverPin && driverPin->getObjectType() == odb::dbITermObj) {
        odb::dbInst* drivingInst = (static_cast<odb::dbITerm*>(driverPin))->getInst();
        cell_cap_delay_map[drivingInst->getConstName()] = std::make_pair(
            builder->source_cap, builder->source_delay);
      }
    }
  }

  std::vector<Segment*> segs;
  int reset_child_count = 0;
  _clock.forEachSink([&](ClockInst& sink) {
    GridPoint sink_location((double) sink.getX(), (double)sink.getY());
    Segment* seg = new Segment(sink_location, sink_location);
    seg->cap = sink.getInputCap();
    seg->sinkInst = &sink;
    seg->fanout = 1;

    odb::dbInst* instance = sink.getDbInputPin()->getInst();
    if ((instance->isCore() || instance->isPad()) && cell_cap_delay_map.size() > 0) {
      auto pos = cell_cap_delay_map.find(instance->getConstName());
      if (pos != cell_cap_delay_map.end()) {
        reset_child_count ++;

        auto cap_delay = pos->second;
        seg->delay = cap_delay.second;
        seg->gate_cell = true;

        sta::LibertyCell* liberty_cell = _network->libertyCell(instance);
        if (liberty_cell != nullptr && liberty_cell->isBuffer()) {
          seg->cap = sink.getInputCap();
        } else {
          seg->cap += cap_delay.first;
        }

        //TODO: gate cell delay
        if (liberty_cell != nullptr) {
            auto result = lrl::DelayCalc::maxGateDelay(liberty_cell, _options->getBufferTestInSlew(),
                                                       seg->cap, reinterpret_cast<sta::Sta*>(_openSta));
            seg->delay = seg->delay + result.delay;
        }

        logger_->info(CTS, 203, " Reset the load capacitance and delay for cell {} as {} and {}.",
                      instance->getConstName(), seg->cap, seg->delay);
      }
    }

    segs.emplace_back(seg);
  });

  if (this->getChildren().size() != reset_child_count) {
    // make sure all clock gating has new delay/capacitance
    logger_->warn(CTS, 221, "sub tree {}, reset size {}", this->getChildren().size(), reset_child_count);
  }

  TreeTopology* tree_topology = constructTopo(segs);
  buildSolution(tree_topology); // build information for writing buffer instance/net to db

  if (_options->getPlotSolution()) {
    plotSolution(tree_topology);
  }

  delete tree_topology;
}

// z = x+y
static inline double calcHighZ(Segment* g) {
  return std::max(g->p1.z(), g->p2.z());
}

static inline double calcLowZ(Segment* g) {
  return std::min(g->p1.z(), g->p2.z());
}

// w = x-y
static inline double calcHighW(Segment* g) {
  return std::max(g->p1.w(), g->p2.w());
}

static inline double calcLowW(Segment* g) {
  return std::min(g->p1.w(), g->p2.w());
}

// calcDistance refer from paper Equispreading Tree in Manhattan Distance by M.Edahiro
static double calcDistance(Segment* g1, Segment* g2) {
  return std::max(std::max(calcLowZ(g1), calcLowZ(g2)) - std::min(calcHighZ(g1), calcHighZ(g2)),
                  std::max(calcLowW(g1), calcLowW(g2)) - std::min(calcHighW(g1), calcHighW(g2)));
}

static void calcBoundary(TRR& trr, std::vector<GridPoint>& trr_boundary_grid) {
  if (trr.core->slope() > 0) {// clock-wise
    trr_boundary_grid.emplace_back(trr.core->p1.x, trr.core->p1.y - trr.radius);
    trr_boundary_grid.emplace_back(trr.core->p2.x + trr.radius, trr.core->p2.y);
    trr_boundary_grid.emplace_back(trr.core->p2.x, trr.core->p2.y + trr.radius);
    trr_boundary_grid.emplace_back(trr.core->p1.x - trr.radius, trr.core->p1.y);
  } else if (trr.core->slope() < 0) {
    trr_boundary_grid.emplace_back(trr.core->p1.x + trr.radius, trr.core->p1.y);
    trr_boundary_grid.emplace_back(trr.core->p2.x, trr.core->p2.y + trr.radius);
    trr_boundary_grid.emplace_back(trr.core->p2.x - trr.radius, trr.core->p2.y);
    trr_boundary_grid.emplace_back(trr.core->p1.x,trr.core->p1.y - trr.radius);
  } else { // leaf node
    trr_boundary_grid.emplace_back(trr.core->p1.x, trr.core->p1.y - trr.radius);
    trr_boundary_grid.emplace_back(trr.core->p1.x + trr.radius, trr.core->p1.y);
    trr_boundary_grid.emplace_back(trr.core->p1.x, trr.core->p1.y + trr.radius);
    trr_boundary_grid.emplace_back(trr.core->p1.x - trr.radius,trr.core->p1.y);
  }
}

static Segment* trrIntersect(TRR& trr1, TRR& trr2) {
  std::vector<GridPoint> trr1_boundary_grid;
  std::vector<GridPoint> trr2_boundary_grid;
  std::vector<Segment> trr1_sides;
  std::vector<Segment> trr2_sides;

  calcBoundary(trr1, trr1_boundary_grid);
  calcBoundary(trr2, trr2_boundary_grid);

  for (int i = 0; i < 3; i++) {
    trr1_sides.emplace_back(trr1_boundary_grid[i], trr1_boundary_grid[i + 1]);
    trr2_sides.emplace_back(trr2_boundary_grid[i], trr2_boundary_grid[i + 1]);
  }
  trr1_sides.emplace_back(trr1_boundary_grid[3], trr1_boundary_grid[0]);
  trr2_sides.emplace_back(trr2_boundary_grid[3], trr2_boundary_grid[0]);

  std::vector<Segment*> mss;
  // for 4*4 check intersect
  for (auto& seg1 : trr1_sides) {
    for (auto& seg2 : trr2_sides) {
      Segment* seg = seg1.intersect(seg2);
      if (seg != nullptr) {
        bool can_add = true;

        for (auto ms : mss) {
          if (*seg == *ms) {
            can_add = false;
            break;
          }
        }

        if (can_add) {
          mss.emplace_back(seg);
        }
      }
    }
  }

  //return line first
  for (auto ms : mss) {
    if (!(ms->p1 == ms->p2)) {
      return ms;
    }
  }
  if( std::size(mss) > 0 ) {
    return mss[0];
  }

  return nullptr;
}

static Segment* trrIntersectSegment(TRR& trr, Segment& seg)
{
  std::vector<GridPoint> trr_boundary_grid;
  std::vector<Segment> trr_sides;

  calcBoundary(trr, trr_boundary_grid);

  for (int i = 0; i < 3; i++) {
    trr_sides.emplace_back(trr_boundary_grid[i], trr_boundary_grid[i + 1]);
  }
  trr_sides.emplace_back(trr_boundary_grid[3], trr_boundary_grid[0]);

  for (auto& side : trr_sides) {
    Segment* ret = side.intersect(seg);
    if (ret != nullptr) {
      return ret;
    }
  }

  return nullptr;
}

Segment* NNCBuilder::mergeByDelay(Segment* seg1, Segment* seg2) {
  double d = calcDistance(seg1, seg2);

  //check max load capacitance
  double p_cap = seg1->cap + seg2->cap + d * _techChar->getCapPerDBU() / 2.0;
  fixCapByBuffer(seg1, seg2, d, p_cap);

  // check max fanout
  int p_fanout = seg1->fanout + seg2->fanout;
  fixMaxFanoutByBuffer(seg1, seg2, d, p_cap, p_fanout);

  double d1, d2, wire_delay1, wire_delay2;
  balanceDelayByBuffer(seg1, seg2, d, d1, d2, wire_delay1, wire_delay2);

  // reset fanout & cap for merge point
  p_fanout = seg1->fanout + seg2->fanout;
  p_cap = seg1->cap + seg2->cap + _techChar->getCapPerDBU() * d / 2.0;

  TRR trr1 = TRR(seg1, d1);
  TRR trr2 = TRR(seg2, d2);
  Segment* ms = trrIntersect(trr1, trr2);
  if (ms == nullptr) {
    logger_->error(CTS, 210, " Can not find intersection between two TRRs, {} and {}.",trr1, trr2);
  }

  seg1->distance = d1;
  seg2->distance = d2;
  ms->delay = std::max(seg1->delay + wire_delay1, seg2->delay + wire_delay2);
  ms->cap = p_cap;

  // slew degradation from "Fast Algorithms For Slew Constrained Minimum Cost Buffering"
  double s_wire_seg1 = wire_slew_coefficient * wire_delay1;
  double s_wire_seg2 = wire_slew_coefficient * wire_delay2;
  double p_slew_degradation
      = std::max(seg1->slew_degradation + s_wire_seg1, seg2->slew_degradation + s_wire_seg2);

  // need to insert buffer at the root of parent node
  if (p_slew_degradation >= _options->getSlewDegradationBound()) {
    // use slew_degradation to select buffer, need to make sure the children are in the bound

    // choose the smallest delay from the buffers which meet the slew degradation
    double min_buffer_delay = std::numeric_limits<double>::max();
    double min_buffer_in_cap = 0.0;
    odb::dbMaster* min_delay_buffer = nullptr;
    for (odb::dbMaster* buffer : _techChar->getBufferList()) {
      sta::Cell* masterCell = _network->dbToSta(buffer);
      sta::LibertyCell* libertyCell = _network->libertyCell(masterCell);

      // get buffer delay&input cap
      auto result = lrl::DelayCalc::maxGateDelay(libertyCell, _options->getBufferTestInSlew(),
                                                 p_cap, reinterpret_cast<sta::Sta*>(_openSta));

      lrl::ArcDelay buffer_delay = result.delay;
      lrl::Slew out_slew = result.out_slew;

      // calc the slew of the child node
      // Note: delay is calculated from bottom to up
      double slew1 = sqrt(out_slew * out_slew + s_wire_seg1 * s_wire_seg1);
      if (slew1 > _options->getMaxCharSlew()) {
        continue ;
      }
      double slew2 = sqrt(out_slew * out_slew + s_wire_seg2 * s_wire_seg2);
      if (slew2 > _options->getMaxCharSlew()) {
        continue ;
      }

      if (buffer_delay < min_buffer_delay) {
        min_buffer_delay = buffer_delay;
        min_buffer_in_cap = lrl::DelayCalc::bufferInputCap(libertyCell);
        min_delay_buffer = buffer;
      }
    }

    if (min_delay_buffer != nullptr) {
      logger_->info(CTS, 216, " Insert buffer {} because of slew degradation {} exceed the bound {}.",
                    min_delay_buffer->getConstName(), p_slew_degradation, _options->getSlewDegradationBound());

      ms->cap = min_buffer_in_cap;
      ms->delay += min_buffer_delay;
      ms->buffer = min_delay_buffer;

      p_fanout = 1;
      p_slew_degradation = 0.0;
    }
  }

  ms->fanout = p_fanout;
  ms->slew_degradation = p_slew_degradation;

  return ms;
}

void NNCBuilder::balanceDelayByBuffer(Segment* seg1,  Segment* seg2,
                                      double d, double& d1, double& d2,
                                      double& wire_delay1, double& wire_delay2) {
  double delay_gap = calcMergePointByElmore(seg1->delay, seg2->delay,
                                            seg1->cap, seg2->cap, d, d1, d2,
                                            wire_delay1, wire_delay2);

  if (delay_gap > _options->getBalanceDelayThreshold() && (seg1->buffer == nullptr || seg2->buffer == nullptr)) {
    int buffer_count = _techChar->getBufferList().size();
    double min_gap = delay_gap;
    odb::dbMaster* min_buffer_seg1 = nullptr;
    odb::dbMaster* min_buffer_seg2 = nullptr;

    double min_buffer_delay_seg1 = 0;
    double min_buffer_delay_seg2 = 0;
    double min_buffer_cap_seg1 = 0;
    double min_buffer_cap_seg2 = 0;

    for (int i = 0; i <= buffer_count; i++) {
      for (int j = 0; j <= buffer_count; j++) {
        odb::dbMaster* buffer_seg1 = nullptr;
        odb::dbMaster* buffer_seg2 = nullptr;
        double seg1_delay, seg2_delay, seg1_cap, seg2_cap;

        bool insert_buffer = false;
        if (seg1->buffer == nullptr && i < buffer_count) {
          buffer_seg1 = _techChar->getBufferList()[i];
          sta::Cell* masterCell = _network->dbToSta(buffer_seg1);
          sta::LibertyCell* libertyCell = _network->libertyCell(masterCell);

          // get buffer delay&input cap
          auto result = lrl::DelayCalc::maxGateDelay(libertyCell, _options->getBufferTestInSlew(),
                                                     seg1->cap, reinterpret_cast<sta::Sta*>(_openSta));
          seg1_cap = lrl::DelayCalc::bufferInputCap(libertyCell);
          seg1_delay = seg1->delay + result.delay;

          insert_buffer = true;
        } else {
          seg1_delay = seg1->delay;
          seg1_cap = seg1->cap;
        }

        if (seg2->buffer == nullptr && j < buffer_count) {
          buffer_seg2 = _techChar->getBufferList()[j];
          sta::Cell* masterCell = _network->dbToSta(buffer_seg2);
          sta::LibertyCell* libertyCell = _network->libertyCell(masterCell);

          // get buffer delay&input cap
          auto result = lrl::DelayCalc::maxGateDelay(libertyCell, _options->getBufferTestInSlew(),
                                                     seg2->cap, reinterpret_cast<sta::Sta*>(_openSta));
          seg2_cap = lrl::DelayCalc::bufferInputCap(libertyCell);
          seg2_delay = seg2->delay + result.delay;

          insert_buffer = true;
        } else {
          seg2_delay = seg2->delay;
          seg2_cap = seg2->cap;
        }

        if (insert_buffer) {
          double gap = calcMergePointByElmore(seg1_delay, seg2_delay, seg1_cap,
                                              seg2_cap, d, d1, d2, wire_delay1, wire_delay2);
          if (gap < min_gap) {
            min_gap = gap;
            min_buffer_seg1 = buffer_seg1;
            min_buffer_seg2 = buffer_seg2;
            min_buffer_delay_seg1 = seg1_delay;
            min_buffer_delay_seg2 = seg2_delay;
            min_buffer_cap_seg1 = seg1_cap;
            min_buffer_cap_seg2 = seg2_cap;
          }
        }
      }
    }

    if (min_buffer_seg1 != nullptr) {
      seg1->buffer = min_buffer_seg1;
      seg1->delay = min_buffer_delay_seg1;
      seg1->cap = min_buffer_cap_seg1;
      seg1->slew_degradation = 0;
      seg1->fanout = 1;
    }
    if (min_buffer_seg2 != nullptr) {
      seg2->buffer = min_buffer_seg2;
      seg2->delay = min_buffer_delay_seg2;
      seg2->cap = min_buffer_cap_seg2;
      seg2->slew_degradation = 0;
      seg2->fanout = 1;
    }
  }

  // TODO: maybe not a good choice
  if (std::abs(seg1->delay - seg2->delay) > _options->getBalanceDelayThreshold()) {
    // try to insert more buffers to balance delay
    Segment* small_delay_seg;
    Segment* large_delay_seg;
    if (seg1->delay > seg2->delay) {
      small_delay_seg = seg2;
      large_delay_seg = seg1;
    } else {
      small_delay_seg = seg1;
      large_delay_seg = seg2;
    }

    if(small_delay_seg->buffer != nullptr) {
      while (large_delay_seg->delay - small_delay_seg->delay > _options->getMergeDelayThreshold()) {
        // insert a buffer into small delay seg
        sta::Cell* masterCell = _network->dbToSta(_techChar->getCharBuffer());
        sta::LibertyCell* libertyCell = _network->libertyCell(masterCell);

        // get buffer delay&input cap
        auto result = lrl::DelayCalc::maxGateDelay(libertyCell, _options->getBufferTestInSlew(),
                                                   small_delay_seg->cap, reinterpret_cast<sta::Sta*>(_openSta));
        float load_cap = lrl::DelayCalc::bufferInputCap(libertyCell);

        small_delay_seg->more_buffers.push_back(_techChar->getCharBuffer());
        small_delay_seg->delay += result.delay;
        small_delay_seg->cap = load_cap;
      }

      //reorder buffers for easy to build clock nets
      int add_buffer_size = small_delay_seg->more_buffers.size();
      if( add_buffer_size > 0) {
        auto first_buffer = small_delay_seg->buffer;
        small_delay_seg->buffer = small_delay_seg->more_buffers.back();
        small_delay_seg->more_buffers.pop_back();
        std::reverse(small_delay_seg->more_buffers.begin(), small_delay_seg->more_buffers.end());
        small_delay_seg->more_buffers.push_back(first_buffer);

        calcMergePointByElmore(seg1->delay, seg2->delay, seg1->cap,
                               seg2->cap, d, d1, d2, wire_delay1, wire_delay2);


        logger_->info(CTS, 212, " Add {} buffers to balance in clock {}.", add_buffer_size, _clock.getName());
      }
    }
  }
}

void NNCBuilder::fixMaxFanoutByBuffer(Segment* seg1, Segment* seg2, double distance,
                                      double& p_cap, int& p_fanout) {
  int max_fanout = _options->getMaxFanout();
  if (max_fanout > 0 && p_fanout > max_fanout) {
    auto buffer = _techChar->getCharBuffer();
    float seg1_delay = seg1->delay;
    float seg2_delay = seg2->delay;

    bool fix_max_fanout = false;
    if (seg1_delay < seg2_delay) {
      if (seg1->buffer == nullptr) {
        insertBuffer(seg1, buffer);
        fix_max_fanout = true;
      }
    } else if (seg2_delay < seg1_delay) {
      if (seg2->buffer == nullptr) {
        insertBuffer(seg2, buffer);
        fix_max_fanout = true;
      }
    } else {
      if (seg1->buffer == nullptr) {
        insertBuffer(seg1, buffer);
        fix_max_fanout = true;
      }
      if (seg2->buffer == nullptr) {
        insertBuffer(seg2, buffer);
        fix_max_fanout = true;
      }
    }

    if (!fix_max_fanout) {
      logger_->warn(CTS, 214, " Can not fix max fanout with delay {} and {}, with fanout {} and {}.",
                    seg1_delay, seg2_delay, seg1->cap, seg2->cap);
    } else {
      p_fanout = seg1->fanout + seg2->fanout;
      p_cap = seg1->cap + seg2->cap + _techChar->getCapPerDBU() * distance / 2.0;
    }
  }
}

void NNCBuilder::fixCapByBuffer(Segment* seg1, Segment* seg2,
                                double distance, double& p_cap) {
  float max_buffer_load_cap = _options->getBufferMaxLoadCap();
  if (p_cap > max_buffer_load_cap) {
    auto buffer = _techChar->getCharBuffer();
    float seg1_delay = seg1->delay;
    float seg2_delay = seg2->delay;

    bool fix_max_load_cap = false;
    if (seg1_delay < seg2_delay) {
      if (seg1->buffer == nullptr) {
        insertBuffer(seg1, buffer);
        fix_max_load_cap = true;
      }
    } else if (seg2_delay < seg1_delay) {
      if (seg2->buffer == nullptr) {
        insertBuffer(seg2, buffer);
        fix_max_load_cap = true;
      }
    } else {
      if (seg1->buffer == nullptr) {
        insertBuffer(seg1, buffer);
        fix_max_load_cap = true;
      }
      if (seg2->buffer == nullptr) {
        insertBuffer(seg2, buffer);
        fix_max_load_cap = true;
      }
    }

    if (!fix_max_load_cap) {
      logger_->warn(CTS, 213, " Can not fix max load Capacitance with delay {} and {},"
                    "with load cap {} and {}.", seg1_delay, seg2_delay, seg1->cap, seg2->cap);
    } else {
      p_cap = seg1->cap + seg2->cap + _techChar->getCapPerDBU() * distance / 2.0;
    }
  }
}

void NNCBuilder::insertBuffer(Segment* seg, odb::dbMaster* buffer) {
  sta::Cell* masterCell = _network->dbToSta(buffer);
  sta::LibertyCell* libertyCell = _network->libertyCell(masterCell);

  seg->fanout = 1;

  auto result = lrl::DelayCalc::maxGateDelay(libertyCell, _options->getBufferTestInSlew(),
                                             seg->cap, reinterpret_cast<sta::Sta*>(_openSta));
  seg->cap = lrl::DelayCalc::bufferInputCap(libertyCell);
  seg->delay += result.delay;
  seg->buffer = buffer;
  seg->slew_degradation = 0;
}

// calculate the merging distance from Tsay's "An Exact Zero-Skew Clock Routing
// Algorithm"
double NNCBuilder::calcMergePointByElmore(const double seg1_delay, const double seg2_delay,
                                          const double seg1_cap, const double seg2_cap,
                                          double d, double& d1, double& d2,
                                          double& wire_d1, double& wire_d2) {
  double x,y,z;
  y = seg2_delay - seg1_delay
      + _techChar->getResPerDBU() * d * (seg2_cap + _techChar->getCapPerDBU() * d / 2);
  z = _techChar->getResPerDBU() * d * (seg1_cap + seg2_cap + _techChar->getCapPerDBU() * d);
  x = y / z;  // distance(p->seg1)=x*d distance(p->seg2)=(1-x)*d
  if(x <= 0) {
    d1 = 1;
    d2 = d - 1;
    wire_d1 = 0;
    wire_d2 = _techChar->getResPerDBU() * d * (_techChar->getCapPerDBU() * d / 2 + seg2_cap);

    return seg1_delay - seg2_delay - wire_d2;
  } else if (x >= 1) {
    d1 = d - 1;
    d2 = 1;
    wire_d1 = _techChar->getResPerDBU() * d * (_techChar->getCapPerDBU() * d / 2 + seg1_cap);
    wire_d2 = 0;

    return seg2_delay - seg1_delay - wire_d1;
  } else {
    d1 = std::max(int(x * d), 1);
    d2 = d - d1;
    wire_d1 = _techChar->getResPerDBU() * d1 * (_techChar->getCapPerDBU() * d1 / 2 + seg1_cap);
    wire_d2 = _techChar->getResPerDBU() * d2 * (_techChar->getCapPerDBU() * d2 / 2 + seg2_cap);

    return 0;
  }

}

#if 0
static Segment* merge(Segment* seg1, Segment* seg2) {
  double d = calcDistance(seg1, seg2);

  double d1, d2;

  if(std::abs(seg1->delay - seg2->delay) > d) {
    if(seg1->delay > seg2->delay) {
      d1 = 0;
      d2 = d;
    } else {
      d1 = d;
      d2 = 0;
    }
  } else {
    d2 = (double)(seg1->delay - seg2->delay + d)/2.0;
    d1 = d - d2;
  }

  TRR trr1 = TRR(seg1, d1);
  TRR trr2 = TRR(seg2, d2);

  Segment* ms = trrIntersect(trr1, trr2);
  assert(ms != nullptr);

  seg1->distance = d1;
  seg2->distance = d2;
  ms->delay = d2 + seg2->delay;

  return ms;
}
#endif

void deleteSegment(std::vector<Segment*>& segments, Segment* seg) {
  std::vector<Segment*>::iterator it;
  for (it = segments.begin(); it != segments.end();) {
    if (**it == *seg)
      it = segments.erase(it);
    else
      ++it;
  }
}

template< typename T >
struct PtrDeepHash {
  size_t operator()(T* a) const { return a ? a->hash() : 0; }
};

template< typename T >
struct PtrDeepEq {
  size_t operator()(T* a, T* b) const {
    if (a && b) return *a == *b;
    return a == b;
  }
};

void NNCBuilder::localEmbedding(TreeNode* node, int& node_count, int& sink_count) {
  if (node->children.size() == 0) {
    if (node->segment->sinkInst) {
      sink_count ++;
    }
    return ;
  }

  for (TreeNode* child : node->children) {
    node_count++;

    Segment* temp = new Segment(*node->loc, *node->loc);
    TRR trr = TRR(temp, child->segment->distance);
    if ((int)child->segment->distance == 0 ) {
      logger_->warn(CTS, 211, " two nodes at the same point {}.", *node->loc);
    }
    Segment* mp = trrIntersectSegment(trr, *child->segment);
    assert(mp != nullptr);
    child->loc = new GridPoint(mp->p1.x, mp->p1.y);
    delete temp;

    localEmbedding(child, node_count, sink_count);
  }
}

TreeTopology* NNCBuilder::constructTopo(std::vector<Segment*>& segments) {
  TreeTopology* topology = new TreeTopology();

  int max_id = 0;
  for (int i = 0; i < segments.size(); i++) {
    auto &segment = segments[i];
    segment->id = max_id;
    TreeNode* node = new TreeNode(max_id);
    node->segment = segment;
    topology->id_node.emplace(max_id, node);
    max_id ++;
  }

  bool skip_by_delay = true;
  while (segments.size() > 1) {
    std::unordered_map<Segment*, int, PtrDeepHash<Segment>, PtrDeepEq<Segment>> segment_map;
    std::vector<Edge> edges;
    std::vector<Edge> independent_set;

    for (auto segment : segments) {
      segment_map.emplace(segment, segment->id);
    }

    relationGraphByBucket(segments, edges, skip_by_delay);
    if (edges.size() == 0) {
      skip_by_delay = false;
      continue ;
    }

    findIndependentSet(edges, segments.size(), 2, independent_set);

    logger_->info(CTS, 204, " Find {} independent edges from {} edges, remaining segments {}.",
                  independent_set.size(), edges.size(), segments.size());

    for (auto edge : independent_set) {
      if(segment_map.find(edge.nodes[0]) != segment_map.end()
          && segment_map.find(edge.nodes[1]) != segment_map.end()) {
        Segment* ms = mergeByDelay(edge.nodes[0], edge.nodes[1]);

        TreeNode* node;
        if(segment_map.find(ms) != segment_map.end()) {
          int seg_id = segment_map.find(ms)->second;
          node = topology->id_node.find(seg_id)->second;
          if (ms->sinkInst != nullptr) {
            node->segment->sinkInst = ms->sinkInst;
          }
        } else {
          ms->id = max_id;
          segments.emplace_back(ms);
          segment_map.emplace(ms, max_id);
          max_id++;

          //build node info
          node = new TreeNode(ms->id);
          node->segment = ms;
          topology->id_node.emplace(ms->id, node);
        }

        TreeNode* node0 = topology->id_node.find(edge.nodes[0]->id)->second;
        if (!(*(node0->segment) == *ms)) {
          node->children.emplace_back(node0);
          node0->par = node;

          deleteSegment(segments, edge.nodes[0]);
          segment_map.erase(edge.nodes[0]);
        }

        TreeNode* node1 = topology->id_node.find(edge.nodes[1]->id)->second;
        if (!(*(node1->segment) == *ms)) {
          node->children.emplace_back(node1);
          node1->par = node;

          deleteSegment(segments, edge.nodes[1]);
          segment_map.erase(edge.nodes[1]);
        }
      }
    }
  }

  topology->root = topology->id_node.find(segments[0]->id)->second;
  topology->size = topology->id_node.size();

  if (topology->root->segment->buffer == nullptr) { // add buffer into root of tree
    insertBuffer(topology->root->segment, _techChar->getBufferList()[0]);
  }

  logger_->info(CTS, 205, " Tree topology has constructed, node size is {}.", topology->size);

  logger_->info(CTS, 206, " Start to find the exact merge point for clock tree {}.", _clock.getName());

  // Find exact merge point placement(top down)
  GridPoint clock_source = GridPoint(_clock.getClockPinX(), _clock.getClockPinY());
  Segment source_seg = Segment(clock_source, clock_source);
  double distance = calcDistance(&source_seg, topology->root->segment);

  source_cap = topology->root->segment->cap + distance * _techChar->getCapPerDBU() / 2.0;
  source_delay = topology->root->segment->delay + _techChar->getResPerDBU() * distance * source_cap;

  logger_->info(CTS, 207, " Capacitance and delay for the source of clock tree {} is {} and {}.",
                _clock.getName(), source_cap, source_delay);

  TRR trr = TRR(&source_seg, distance);
  Segment* mp = trrIntersectSegment(trr, *topology->root->segment);
  assert(mp != nullptr);
  topology->root->loc = new GridPoint(mp->p1.x, mp->p1.y);

  int node_count = 1;
  int sink_count = 0;
  localEmbedding(topology->root, node_count, sink_count);

  logger_->info(CTS, 208, " Found all the exact merge point for clock tree {}.", _clock.getName());

  logger_->info(CTS, 209, " Total number of nodes: {}, sink count {}.", node_count, sink_count);
  assert(_clock.getNumSinks() == sink_count);

  return topology;
}

#if 0
struct Key {
  int z;
  int w;
  bool operator < (const Key& k) const {
    if (z < k.z) return true;
    if (w < k.w) return true;
    return false;
  }
};
class XMap : public std::map< Key,std::vector<Segment*> > {
  void insert(Segment* s, ) {
    s->z()
  }
  void findZStart(int z) {
    return find(Key(z, -1)) + 1;
  }
  void findZEnd(int z) {
    return find(Key(z, int_max));
  }
};
#endif

bool NNCBuilder::skipMergeByDelay(Segment* g1, Segment* g2, double distance) {
  double delay_gap = 0;

  if (g1->delay > g2->delay) {
    delay_gap = g1->delay - g2->delay- _techChar->getResPerDBU() * distance
                      * (_techChar->getCapPerDBU() * distance + g2->cap);
  } else if (g1->delay < g2->delay) {
    delay_gap = g2->delay - g1->delay- _techChar->getResPerDBU() * distance
                                            * (_techChar->getCapPerDBU() * distance + g1->cap);
  } else {
    return false;
  }

  if (delay_gap > _options->getMergeDelayThreshold()) {
    return true;
  } else {
    return false;
  }
}

void NNCBuilder::relationGraphByBucket(std::vector<Segment*> segs,
                                       std::vector<Edge>& edges, bool skip_by_delay) {
  double z_max = std::numeric_limits<double>::min();
  double w_max = std::numeric_limits<double>::min();
  double z_min = std::numeric_limits<double>::max();
  double w_min = std::numeric_limits<double>::max();

#define MIN_ALL(a, b, c) \
  std::min(std::min(b, c), a)

#define MAX_ALL(a, b, c) \
  std::max(std::max(b, c), a)

  // calculate the diameter of all points(segments)
  for (auto seg : segs) {
    double z1 = seg->p1.z();
    double z2 = seg->p2.z();
    double w1 = seg->p1.w();
    double w2 = seg->p2.w();

    z_min = MIN_ALL(z_min, z1, z2);
    w_min = MIN_ALL(w_min, w1, w2);
    z_max = MAX_ALL(z_max, z1, z2);
    w_max = MAX_ALL(w_max, w1, w2);
  }
#undef MIN
#undef MAX

  // calculate the bucket number and bucket size
  double D = std::max(z_max - z_min, w_max - w_min);
  int K = segs.size();
  int s = std::max(graph_k, int(std::ceil(K * (1.0 - 1.0 / float(graph_k)))));
  int d = int(std::ceil(std::sqrt(float(s))) - 1);
  int size = std::max(int(std::ceil(float(D) / float(d))), 1);

  // calculate the buckets which point(segment) belong to, segment may cross
  // multiple buckets [z][w][]seg
  std::vector<Segment*> bucket_segs[d+1][d+1];

  for (auto g : segs) {
    int z1 = (int)((g->p1.z()) - z_min) / size;
    int z2 = (int)((g->p2.z()) - z_min) / size;
    int w1 = (int)((g->p1.w()) - w_min) / size;
    int w2 = (int)((g->p2.w()) - w_min) / size;

    int max_z = std::max(z1, z2);
    int max_w = std::max(w1, w2);
    for (int z = std::min(z1, z2); z <= max_z; z++) {
      for (int w = std::min(w1, w2); w <= max_w; w++) {
        bucket_segs[z][w].push_back(g);
      }
    }
  }

  // for each point(segment), calculate the min distance from points(segments)
  // in around 9 buckets
  for (auto g : segs) {
    int z1 = (int)((g->p1.z()) - z_min) / size;
    int z2 = (int)((g->p2.z()) - z_min) / size;
    int w1 = (int)((g->p1.w()) - w_min) / size;
    int w2 = (int)((g->p2.w()) - w_min) / size;

    //TODO: too many nested loop
    double min_distance = std::numeric_limits<double>::max();
    Segment* min_seg;
    bool has_neighbor = false;
    int max_z = std::min(d, std::max(z1, z2) + 1);
    int max_w = std::min(d, std::max(w1, w2) + 1);
    for (int z = std::max(0, std::min(z1, z2) - 1); z <= max_z; z++) {
      for (int w = std::max(0, std::min(w1, w2) - 1); w <= max_w; w++) {
        for (auto near_seg : bucket_segs[z][w]) {
          if (g == near_seg) {
            continue;
          }

          // cost == distance, ignore the initial delay of the leaf node
          double distance = calcDistance(g, near_seg);

          if (skip_by_delay && skipMergeByDelay(g, near_seg, distance)) {
            continue ;
          }
          if (distance < min_distance) {
            min_distance = distance;
            min_seg = near_seg;
            has_neighbor = true;
          }
        }
      }
    }

    if (has_neighbor) {
      Edge edge = Edge();
      edge.cost = min_distance;
      edge.nodes.push_back(g);
      edge.nodes.push_back(min_seg);

      edges.emplace_back(edge);
    }
  }
}

bool checkIndependent(std::vector<Edge>& edges, Edge& edge) {
  if (edges.size() == 0) {
    return true;
  }

  for (auto each : edges) {
    if (each.depended(edge)) {
      return false;
    }
  }

  return true;
}

bool sortByCost(Edge a, Edge b) {
  return a.cost < b.cost;
}

void NNCBuilder::findIndependentSet(std::vector<Edge>& edges,
                                    int K, int k, std::vector<Edge>& indep_edges) {
  size_t count = size_t(std::floor(float(K) / (float(k))));
  std::sort(edges.begin(), edges.end(), sortByCost);

  for (int i = 0; i < std::min(count, edges.size()); i++) {
    if (checkIndependent(indep_edges, edges[i])) {
      indep_edges.push_back(edges[i]);
    }
  }
}


void NNCBuilder::buildSolutionInner(TreeNode* node, std::queue<TreeNode*>& buffer_queue,
                                    std::vector<TreeNode*>& net_nodes) {
  if (node->segment->sinkInst) {
    net_nodes.push_back(node);
  }

  if (node->segment->buffer) {
    buffer_queue.push(node);
    net_nodes.push_back(node);

    _clock.addClockBuffer("buffer_" + std::to_string(node->id),
                          node->segment->buffer->getConstName(), node->loc->x, node->loc->y);

    return ;
  }

  for (auto child : node->children) {
    buildSolutionInner(child, buffer_queue, net_nodes);
  }

}

void NNCBuilder::buildSolution(TreeTopology* tree) {
  std::queue<TreeNode*> buffer_queue;

  TreeNode* node = tree->root;
  buffer_queue.push(node);
  //add root buffer
  _clock.addClockBuffer("clkbuf_0", node->segment->buffer->getConstName(), node->loc->x, node->loc->y);
  int net_count = 0;

  while(!buffer_queue.empty()) {
    node = buffer_queue.front();
    buffer_queue.pop();

    //以node为根节点，找每条边的终点(sink or buffer), 如果是buffer则加入队列中继续计算
    std::vector<TreeNode*> net_nodes;
    net_nodes.push_back(node);

    if (node->children.size() > 0) {
      for (auto child : node->children) {
        buildSolutionInner(child, buffer_queue, net_nodes);
      }
    }

    double interval_x = 0, interval_y = 0;
    if (node->segment->more_buffers.size() > 0) {
      double total_x = 0, total_y = 0;
      for (auto net_node : net_nodes) {
        if (net_node == node) {
          continue ;
        }

        total_x += net_node->loc->x;
        total_y += net_node->loc->y;
      }

      double centroid_x = 0, centroid_y = 0;
      if (net_nodes.size() == 1) {
        centroid_x = total_x;
        centroid_y = total_y;
      } else {
        centroid_x = total_x / (net_nodes.size() - 1);
        centroid_y = total_y / (net_nodes.size() - 1);
      }

      interval_x = (node->loc->x - centroid_x) / (node->segment->more_buffers.size() + 1);
      interval_y = (node->loc->y - centroid_y) / (node->segment->more_buffers.size() + 1);
    }

    Clock::SubNet& clock_sub_net = _clock.addSubNet("net_" + std::to_string(net_count));
    net_count ++;
    int new_net_count = 0;
    for (auto net_node : net_nodes) {
      if (net_node->segment->buffer) {
        auto instance = _clock.findClockByName(_clock.getName() + "_buffer_" + std::to_string(net_node->id));
        if (net_node == tree->root) {
          instance = _clock.findClockByName(_clock.getName() + "_clkbuf_0");
        }
        assert(instance != nullptr);

        if (net_node->segment->more_buffers.size() > 0 && net_node == node) {
          int buffer_size = net_node->segment->more_buffers.size();

          for(int i = 0; i < buffer_size; i ++) {
            new_net_count ++;

            auto buffer_name = "buffer_balance_" + std::to_string(net_node->id) + "_" + std::to_string(i);
            _clock.addClockBuffer(buffer_name,
                                  net_node->segment->more_buffers[i]->getConstName(),
                                  net_node->loc->x - interval_x * (i + 1),
                                  net_node->loc->y - interval_y * (i + 1));

            auto new_net_name = "net_balance_" + std::to_string(net_count) + "_" + std::to_string(new_net_count);
            Clock::SubNet& new_clock_sub_net = _clock.addSubNet(new_net_name);

            auto current_instance = _clock.findClockByName(_clock.getName() + "_" + buffer_name);
            if (i == 0) {
              new_clock_sub_net.addInst(*instance);
            } else {
              auto pre_buffer_name = _clock.getName() + "_buffer_balance_" + std::to_string(net_node->id)
                                     + "_" + std::to_string(i-1);
              auto pre_instance = _clock.findClockByName(pre_buffer_name);
              new_clock_sub_net.addInst(*pre_instance);
            }
            new_clock_sub_net.addInst(*current_instance);
          }

          auto buffer_name = "buffer_balance_" + std::to_string(net_node->id)
                             + "_" + std::to_string(buffer_size-1);
          auto new_instance = _clock.findClockByName(_clock.getName() + "_" + buffer_name);
          clock_sub_net.addInst(*new_instance);
        } else {
          clock_sub_net.addInst(*instance);
        }
      }
      if (net_node->segment->sinkInst) {
        clock_sub_net.setLeafLevel(true);
        clock_sub_net.addInst(*net_node->segment->sinkInst);
      }
    }
  }
}

std::string colors[] = {"k","lime","g","b","c","m","orange","yellow"};

void plotTreeInner(std::ofstream &file, TreeNode* node, int i, int& total) {
  std::string color = "red";
  if (i < 8) {
    color = colors[i];
  }
  if (node->children.size() == 0) {
    if (node->segment->gate_cell) {
      file << "plt.scatter(" << node->loc->x << ", " << node->loc->y  << ", s=500)\n";
    } else {
      file << "plt.scatter(" << node->loc->x << ", " << node->loc->y  << ", s=10)\n";
    }
  } else {
    int depth = i + 1;
    for (TreeNode* child : node->children) {
      total ++;
      if (child->segment->buffer) {
        file << "plt.scatter(" << child->loc->x << ", "
             << child->loc->y  << ", s=50,marker='v')\n";
      } else {
        file << "plt.scatter(" << child->loc->x << ", "
             << child->loc->y  << ", s=10,marker='x')\n";
      }
      if (depth <= 8) {
        file << "plt.plot([" << node->loc->x << ", "
             << child->loc->x << "], [" << node->loc->y
             << ", " << child->loc->y << "], c = '" << color <<"')\n";
      }
      plotTreeInner(file, child, depth, total);
    }
  }
}

void NNCBuilder::plotSolution(TreeTopology* tree) {
  int i = 0;
  int total = 1;
  auto name = std::string("plot") + _clock.getName() + ".py";
  std::ofstream file(name);
  file << "import numpy as np\n";
  file << "import matplotlib.pyplot as plt\n";
  file << "import matplotlib.path as mpath\n";
  file << "import matplotlib.lines as mlines\n";
  file << "import matplotlib.patches as mpatches\n";
  file << "from matplotlib.collections import PatchCollection\n\n";

  file << "plt.scatter(" << _clock.getClockPinX() << ", "
       << _clock.getClockPinY()  << ", s=10)\n";
  file << "plt.plot([" << _clock.getClockPinX() << ", "
       << tree->root->loc->x << "], [" << _clock.getClockPinY()
       << ", " << tree->root->loc->y << "], c = 'k')\n";
  if (tree->root->segment->buffer) {
    file << "plt.scatter(" << tree->root->loc->x << ", "
         << tree->root->loc->y  << ", s=50,marker='v')\n";
  } else {
    file << "plt.scatter(" << tree->root->loc->x << ", "
         << tree->root->loc->y  << ", s=10)\n";
  }

  plotTreeInner(file, tree->root, i, total);

  file << "plt.show()\n";
  file.close();
}

}  // namespace cts
