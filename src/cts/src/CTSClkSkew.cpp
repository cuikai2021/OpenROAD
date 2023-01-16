#include "CTSClkSkew.h"

#include "db_sta/dbSta.hh"
#include "sta/Sdc.hh"

namespace cts {

sta::ClkSkewMap CTSClkSkew::findClkSkews() {
  _openSta->ensureClkArrivals();
  sta::ClockSet clks;
  for (sta::Clock* clk : *(_openSta->sdc())->clocks())
    clks.insert(clk);
  sta::ClkSkewMap skews;
  findClkSkew(&clks, nullptr, sta::SetupHold::max(), skews);
  return skews;
}

CTSClkSkew::CTSClkSkew(sta::dbSta* sta) : ClkSkews(sta) {
  _openSta = sta;
}

}  // namespace cts