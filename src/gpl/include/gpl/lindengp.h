#pragma once

#include <memory>
#include <string>
#include <deque>

#include "linden/db/db.hpp"
#include "linden/gpl/placer.hpp"

namespace odb {
class dbDatabase;
}  // namespace odb

namespace utl {
class Logger;
}

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

  void setTargetDensity(float density) { density_ = density; }
  void setTargetOverflow(float overflow) { overflow_ = overflow; }
  void setUniformDensity(bool use_uniform_density)
  {
    use_uniform_density_ = use_uniform_density;
  }

  void setZigzag(bool zigzag)
  {
    zigzag_ = zigzag;
  }

  void setPadLeft(int padding) { padLeft_ = padding; }
  void setPadRight(int padding) { padRight_ = padding; }

  void setTimingDriven(bool timing_driven);

  void setDebug(int pause_iterations, int update_iterations, bool initial)
  {
    gui_debug_ = true;
    gui_debug_pause_iterations_ = pause_iterations;
    gui_debug_update_iterations_ = update_iterations;
    gui_debug_initial_ = initial;
  }

  void setInitialPlacePolicy(const char* policy);

 private:
  odb::dbDatabase* db_;
  utl::Logger* log_;

  std::shared_ptr<PlacerBase> pb_;
  std::string initialPlacePolicy_;
  std::shared_ptr<linden::db::Database> linden_db_;
  std::unique_ptr<linden::gpl::Placer> lindengp_;

  float density_;
  float overflow_;
  bool use_uniform_density_;
  bool zigzag_;

  double net_weight_max_;
  std::deque<int> timing_overflows_;


  // temp variable; OpenDB should have these values.
  int padLeft_;
  int padRight_;

  bool gui_debug_;
  int gui_debug_pause_iterations_;
  int gui_debug_update_iterations_;
  bool gui_debug_initial_;

  // reset to release memory after gp
  void reset();

  void initPlacerBase();
  std::shared_ptr<linden::db::Database> create_linden_db();

  // Timing-driven
  // return False if no slacks found
  bool updateNetWeights();

  void update_db();
  void report_metrics();

  // initial placement
  void doInitialPlace();

  // routability-driven placement
  void test_baby_banyan();
};
}  // namespace gpl
