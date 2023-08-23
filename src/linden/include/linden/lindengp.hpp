#pragma once

#include <deque>
#include <memory>
#include <string>

#include "linden/db/db.hpp"
#include "linden/gpl/placer.hpp"
#include "linden/rdp/congestion_estimation.hpp"

namespace odb {
class dbDatabase;
}  // namespace odb

namespace utl {
class Logger;
}

namespace metrics {
class Reporter;
} // namespace metrics

namespace gpl {
class PlacerBase;

class LindenGlobalPlace
{
 public:
  LindenGlobalPlace();
  ~LindenGlobalPlace() = default;

  void init(odb::dbDatabase* odb, utl::Logger* log)
  {
    db_ = odb;
    log_ = log;
  }

  void run();

  // run in cgr(congestion-repair) mode
  double cgr_run();

  void setTargetDensity(float density) { density_ = density; }
  void setTargetOverflow(float overflow) { overflow_ = overflow; }
  void setUniformDensity(bool use_uniform_density)
  {
    use_uniform_density_ = use_uniform_density;
  }

  void setPadLeft(int padding) { padLeft_ = padding; }
  void setPadRight(int padding) { padRight_ = padding; }

  void setTimingDriven(bool flag);
  void setRoutabilityDriven(bool flag);

  void setDebug(int pause_iterations, int update_iterations, bool initial)
  {
    gui_debug_ = true;
    gui_debug_pause_iterations_ = pause_iterations;
    gui_debug_update_iterations_ = update_iterations;
    gui_debug_initial_ = initial;
  }

  void setInitialPlacePolicy(const char* policy);

  void setFocusInsts(const char* focus_insts_file_name);

  void setFocusNets(const char* focus_nets_file_name);

 private:
  odb::dbDatabase* db_;
  utl::Logger* log_;

  std::shared_ptr<PlacerBase> pb_;
  std::shared_ptr<linden::db::Database> linden_db_;
  std::unique_ptr<linden::gpl::Placer> lindengp_;
  std::shared_ptr<linden::rdp::CongestionMap> banyan_cmap_result_;

  std::string initialPlacePolicy_;

  float density_;
  float overflow_;
  bool use_uniform_density_;

  bool cgr_mode_;

  double net_weight_max_;
  std::deque<int> timing_overflows_;
  bool routability_driven_;

  // temp variable; OpenDB should have these values.
  int padLeft_;
  int padRight_;

  bool gui_debug_;
  int gui_debug_pause_iterations_;
  int gui_debug_update_iterations_;
  bool gui_debug_initial_;

  // case 1: inst names from configuration file
  // case 2: inst names from focusing nets
  std::vector<std::vector<std::string>> focus_inst_names_;

  void setFocusInstNamesFromNets(
      const std::vector<std::vector<std::string>>& net_names);

  // initialize
  void init();

  // reset to release memory after gp
  void reset();

  void initPlacerBase();
  std::shared_ptr<linden::db::Database> create_linden_db();

  // Timing-driven
  // return False if no slacks found
  bool updateNetWeights();

  void update_db();
  void report_metrics(metrics::Reporter&);

  // initial placement
  void doInitialPlace();

  // routability-driven placement
  bool rdp_cmap(const double overflow);
  bool congestion_estimate();

  // change to cgr(congestion-repair) mode
  void change_to_cgr_mode();
};
}  // namespace gpl
