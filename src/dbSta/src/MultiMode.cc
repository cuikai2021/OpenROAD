#include "db_sta/dbSta.hh"
#include "utl/Logger.h"
// #include "sta/Graph.hh"
#include "sta/Sdc.hh"
// #include "sta/Corner.hh"
#include "sta/Search.hh"
#include "parasitics/ConcreteParasitics.hh"
#include "search/CheckMinPulseWidths.hh"
#include "search/CheckMinPeriods.hh"
#include "search/Levelize.hh"
#include "search/Sim.hh"
// #include "search/ReportPath.hh"
#include "dcalc/GraphDelayCalc1.hh"

namespace sta {

void dbSta::backupMode()
{
  if (!sta_map_.hasKey(opt_sdc_name_)){
    debugPrint(logger_, utl::STA, "MMMC", 1, "create SDC: {}", opt_sdc_name_.c_str());
    sta_map_.insert(opt_sdc_name_, sdc_);
  }
}

void dbSta::switchMode()
{
  search_->clear();
  levelize_->clear();
  if (parasitics_)
    parasitics_->clear();
  graph_delay_calc_->clear();
  sim_->clear();
  if (check_min_pulse_widths_)
    check_min_pulse_widths_->clear();
  if (check_min_periods_)
    check_min_periods_->clear();
  // delete graph_;
  // graph_ = nullptr;
  graph_sdc_annotated_ = false;
  current_instance_ = nullptr;
  updateComponentsState();
}

Sdc* dbSta::getOrCreateSdcOpt(std::string const& name)
{
  backupMode();
  opt_sdc_name_ = name;
  if (sta_map_.hasKey(name))
    sdc_ = sta_map_.at(name);
  else
    makeSdc();
  switchMode();
  return sdc_;
}

void dbSta::deleteModes()
{
    for(auto& s : sta_map_)
    {
      Sdc* sdc = s.second;
      if(sdc && sdc != sdc_)
        delete sdc;
    }
}

void dbSta::setSdcOpt(std::string const& name)
{
  getOrCreateSdcOpt(name);
  debugPrint(logger_, utl::STA, "MMMC", 1, "switch SDC: {}", name.c_str());
}

void dbSta::forEachSdc(const std::function<void()>& func)
{
  for (auto& s : sta_map_) {
    Sdc* sdc = s.second;
    std::string const& name = s.first;
    if(sdc && sdc->clocks()->size() > 0)
    {
      setSdcOpt(name);
      func();
    }
    else
      debugPrint(logger_, utl::STA, "MMMC", 1, "ignore SDC: {} , nullptr or no clock defined",
              name.c_str());
  }
}

} // namespace sta