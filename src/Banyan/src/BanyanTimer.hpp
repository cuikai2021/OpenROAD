#pragma once

#include <memory>

#include "banyan_db.pb.h"
#include "banyan_timer.pb.h"

namespace sta{
class dbNetwork;
class Pin;
}

namespace odb {
class dbDatabase;
}

namespace utl {
class Logger;
}

namespace lrl {
class Laurel;
}

namespace banyan {

constexpr int METER_TO_MICRON = 1000000;

class BanyanTimer
{
 private:
  std::unique_ptr<TimingReport> timingReport;

 protected:
  odb::dbDatabase* db_;
  utl::Logger* logger_;
  lrl::Laurel* laurel_;
  std::unique_ptr<Design>& design_;

  sta::Pin* pin2StaPin(sta::dbNetwork* network, Pin* pin);
  void initWireBudgeting();

 public:
  BanyanTimer(odb::dbDatabase* db, utl::Logger* logger, lrl::Laurel* laurel, std::unique_ptr<Design>& design)
      : timingReport(std::make_unique<TimingReport>()), db_(db), logger_(logger), laurel_(laurel), design_(design)
  {
    initWireBudgeting();
  }

  ~BanyanTimer() = default;

  std::unique_ptr<TimingReport> getTimeReport() { return std::move(timingReport); }
};
}  // namespace banyan