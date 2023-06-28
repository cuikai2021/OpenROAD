#pragma once

#include "netModel.h"

namespace gpl {

class Pin;

// Bound2Bound Net Model
class NetModelB2B : public NetModel
{
 public:
  NetModelB2B(size_t minDiffLength,
              float netWeightScale,
              const IsScanChainsIgnoredPin& is_scan_chains_ignored_pin,
              const IgnoredNets& ignored_nets,
              const std::vector<Net*>& nets,
              const std::vector<Instance*>& placeInsts)
      : NetModel(netWeightScale, is_scan_chains_ignored_pin,ignored_nets, nets, placeInsts),
        minDiffLength_(minDiffLength)
  {
    build();
  }

 protected:
  virtual void buildOn(DIMENSION dim);
  virtual float weight(Pin* pin1,
                       Pin* pin2,
                       float netWeight,
                       DIMENSION dim) const;
  bool isBoundary(Pin* pin, DIMENSION dim) const;

 private:
  size_t minDiffLength_;
};

}  // namespace gpl