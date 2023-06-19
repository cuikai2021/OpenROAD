#pragma once

#include <memory>
#include <string>
#include <deque>

#include "linden/db/db.hpp"
#include "linden/gpl/placer.hpp"
#include "linden/rdp/congestion_estimation.hpp"

namespace odb {
class dbDatabase;
}  // namespace odb

namespace utl {
class Logger;
}

namespace cgr {
class PlacerBase;

class CongestionRepair
{
 public:
  CongestionRepair();
  ~CongestionRepair() = default;

  void init(odb::dbDatabase* odb, utl::Logger* log)
  {
    db_ = odb;
    log_ = log;
  }

  double run();

  // done and load the best result of cgr runs
  void done() const;

  // get/set cgr params file (which is in protobuf format)
  std::string getCGRParamsFile() const noexcept { return cgr_params_file_; }
  void setCGRParamsFile(const char* cgr_params_file);

  std::string getRoutingMaxLayer() const noexcept { return routing_max_layer_; }
  void setRoutingMaxLayer(const char* max_layer);
  
 private:
  odb::dbDatabase* db_;
  utl::Logger* log_;
  std::string cgr_params_file_;
  std::string routing_max_layer_;

  struct DEF2Congestion {
    DEF2Congestion(std::string& s, double v) : def_file(s), congestion(v) {}
    std::string def_file;
    double congestion;
  };
  std::vector<DEF2Congestion> results_;
};

}  // namespace cgr
