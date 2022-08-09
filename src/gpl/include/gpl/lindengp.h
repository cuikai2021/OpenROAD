#pragma once

#include <memory>
#include <vector>
#include <string>

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

  void init(odb::dbDatabase* odb, utl::Logger* log) { 
    db_ = odb;
    log_ = log; 
  }

  void run();

  void setTargetDensity(float density) { density_ = density; }
  void setTargetOverflow(float overflow)  { overflow_ = overflow; }
  void setUniformDensity(bool use_uniform_density) { use_uniform_density_ = use_uniform_density; }

  void setPadLeft(int padding) { padLeft_ = padding; }
  void setPadRight(int padding) { padRight_ = padding; }

  void setDebug(int pause_iterations, int update_iterations) {
    gui_debug_ = true;
    gui_debug_pause_iterations_ = pause_iterations;
    gui_debug_update_iterations_ = update_iterations;
  }

 private:
  odb::dbDatabase* db_;
  utl::Logger* log_;

  std::shared_ptr<PlacerBase> pb_;
  std::shared_ptr<linden::db::Database> linden_db_;
  std::unique_ptr<linden::gpl::Placer> lindengp_;

  float density_;
  float overflow_;
  bool use_uniform_density_;

  // temp variable; OpenDB should have these values.
  int padLeft_;
  int padRight_;

  bool gui_debug_;
  int gui_debug_pause_iterations_;
  int gui_debug_update_iterations_;

  void initPlacerBase();
  std::shared_ptr<linden::db::Database> create_linden_db();
};
}  // namespace gpl
