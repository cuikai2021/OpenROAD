#pragma once

#include "netModel.h"

namespace gpl {

class Pin;
class Net;

// Star Net Model
class NetModelStar : public NetModel
{
 public:
  NetModelStar(size_t maxFanout,
               float netWeightScale,
               const std::vector<Net*>& nets,
               const std::vector<Instance*>& placeInsts)
      : NetModel(maxFanout, netWeightScale, nets, placeInsts)
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