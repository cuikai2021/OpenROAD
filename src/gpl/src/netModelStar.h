#pragma once

#include "netModel.h"

namespace gpl {

class Pin;
class Net;

// Star Net Model
class NetModelStar : public NetModel
{
 public:
  NetModelStar(float netWeightScale,
               const std::vector<Net*>& nets,
               const std::vector<Instance*>& placeInsts,
               const IsScanChainsIgnoredPin& is_scan_chains_ignored_pin,
               const IgnoredNets& ignored_nets)
      : NetModel(netWeightScale, nets, placeInsts, is_scan_chains_ignored_pin,ignored_nets)
  {
    build();
  }

 protected:
  virtual void buildOn(DIMENSION dim);
  virtual float weight(Pin* pin1,
                       Pin* pin2,
                       float netWeight,
                       DIMENSION dim) const;

  bool isSourcePin(Pin* pin) const;
  Pin* getSourcePin(Net* net) const;
};

}  // namespace gpl