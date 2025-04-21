
#pragma once

#include <numeric>
#include "grt/GlobalRouter.h"
#include "sta/Corner.hh"

namespace grt {

namespace RCTable {

template<typename T>
bool fEqual(const T& a, const T& b) {
  const double epsilon = 1e-5;
  return fabs(a - b) < epsilon;
}

// unitRC for wire
struct RCValue
{
  double res;
  double coupling_cap, gnd_cap;
  RCValue() : res(0.0), coupling_cap(0.0), gnd_cap(0.0) { }
  RCValue(double res, double coupling_cap, double gnd_cap) : res(res), coupling_cap(coupling_cap), gnd_cap(gnd_cap) { }
  void operator+=(const RCValue& oth) {
    this->res += oth.res;
    this->coupling_cap += oth.coupling_cap;
    this->gnd_cap += oth.gnd_cap;
  }
  RCValue operator*(double k) {
    RCValue ret = *this;
    ret.res *= k;
    ret.coupling_cap *= k;
    ret.gnd_cap *= k;
    return ret;
  }
  // fF per_nm
  double unitCap() const { return coupling_cap + gnd_cap; }
  // ohms per_nm
  double unitRes() const { return res; }
  void merge(const RCValue& oth) {
    res = (res + oth.res) / 2.0;
    coupling_cap = (coupling_cap + oth.coupling_cap) / 2.0;
    gnd_cap = (gnd_cap + oth.gnd_cap) / 2.0;
  }
  void merge(const RCValue& oth, double ratio) {
    res = res + (oth.res - res) * ratio;
    coupling_cap = coupling_cap + (oth.coupling_cap - coupling_cap) * ratio;
    gnd_cap = gnd_cap + (oth.gnd_cap - gnd_cap) * ratio;
  }
  operator std::pair<double, double>() const {
    return {unitRes(), unitCap()};
  }
};

class RCTable
{
 private:
// nm
using Spacing = int;
using Width = int;
using SpRCTable = std::map<Spacing, RCValue>;
using WidthSpRCTable = std::map<Width, SpRCTable>;
// over under model
using LaOU = std::tuple<LayerId, LayerId, LayerId>;
using LaOUWiSpRCTable = std::map<LaOU, WidthSpRCTable>;
// only current layer model
// using LaWiSpRCTable = std::vector<WidthSpRCTable>;
 
 public:
  RCTable(odb::dbTech* tech, odb::dbBlock* block, utl::Logger* logger, const char* file_name, sta::Corner* corner) : 
    tech_(tech), block_(block), logger_(logger),  corner_(corner) {
      readRCFromRCXModel(file_name);
    }
  void readRCFromRCXModel(const char* file_name);
  void parseRes(std::ifstream&, LayerId);
  void parseCap(std::ifstream&, LayerId, LayerId, LayerId);

  RCValue getWireUnitRC(LayerId idx_of_layer, Spacing sp, Width width, std::vector<double> densities, bool dbg=false);
  // not support now
  RCValue getViaUnitRC() { return {1, 0, 0}; }
 private:
  RCValue getWireUnitRC(LayerId layer_id, Spacing sp, Width width, LayerId over, LayerId under);
  void checkLoaded(const char*);
  odb::dbTech* tech_;
  odb::dbBlock* block_;
  utl::Logger* logger_;
  sta::Corner* corner_;
  LaOUWiSpRCTable table_;

  const double rc_max_scale_for_overflow = 1.2;
};

class GCellRC
{
 public:
  GCellRC(odb::dbTech* tech, odb::dbBlock* block, GlobalRouter* grouter, Grid* grid, utl::Logger* logger) : 
    tech_(tech), block_(block), grouter_(grouter), grid_(grid), logger_(logger) {};
  void readRCFromRCXModel(const char* file_name, sta::Corner* corner) {
    auto rc_table = new RCTable(tech_, block_, logger_, file_name, corner);
    auto corner_idx = corner->index();
    if (corner_idx >= table_.size()) {
      table_.resize(corner_idx + 1, nullptr);
    }
    if (hasCorner(corner)) {
      logger_->warn(utl::GRT, 1053, "has already read corner {}, will cover prev model", corner->name());
      delete table_[corner_idx];
    }
    table_[corner_idx] = rc_table;
  }
  bool hasCorner(sta::Corner* corner) {
    return table_[corner->index()] != nullptr;
  }
  std::pair<double, double> getSegRC(GSegment g_seg,
                                     sta::Corner* corner,
                                     int wire_length_dbu,
                                     int wire_width_dbu=0,
                                     bool dbg=false);

  ~GCellRC() {
    for (auto rc_table : table_) {
      delete rc_table;
    }
  }
 protected:
  odb::dbTech* tech_;
  odb::dbBlock* block_;
  std::vector<RCTable*> table_;
  GlobalRouter* grouter_;
  Grid* grid_;
  utl::Logger* logger_;
};

}

}  // namespace grt
