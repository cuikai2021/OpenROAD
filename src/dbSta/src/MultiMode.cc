#include "db_sta/dbSta.hh"
#include "utl/Logger.h"
#include "sta/Search.hh"
#include "sta/Sdc.hh"
#include "search/Sim.hh"

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
  // delaysInvalid();
  search_->arrivalsInvalid();
  sim_->clear();
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
  // if it is single mode, avoid redo sta
  if(sta_map_.empty())
    return func();

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