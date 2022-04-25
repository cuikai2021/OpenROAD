#pragma once

#include "netModel.h"

namespace gpl {

class Pin;
class Net;

// Star Net Model
class NetModelStar : public NetModel
{
 public:
  NetModelStar(std::size_t maxFanout,
               float netWeightScale,
               const std::vector<Net*>& nets,
               const std::vector<Instance*>& placeInsts)
      : NetModel(maxFanout, netWeightScale, nets, placeInsts)
  {
    build();
  }

 protected:
  virtual void buildOn(DIMENSION dim);
  virtual float weight(Pin* pin1, Pin* pin2, float netWeight, DIMENSION dim);

  bool isSourcePin(Pin* pin);
  Pin* getSourcePin(Net* net);
};

}  // namespace gpl