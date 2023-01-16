
#pragma once

#include "search/ClkSkew.hh"

namespace sta {
class dbSta;
class Clock;

class ClkSkew
{
 public:
  float skew() const;
};
}  // namespace sta

namespace cts {

class CTSClkSkew : public sta::ClkSkews {
 public:
  CTSClkSkew(sta::dbSta* sta);
  sta::ClkSkewMap findClkSkews();

 private:
  sta::dbSta* _openSta;
};

}  // namespace cts
