#pragma once

#include "netModel.h"

namespace gpl {

class Pin;

// Bound2Bound Net Model
class NetModelB2B : public NetModel
{
 public:
  NetModelB2B(size_t minDiffLength,
              size_t maxFanout,
              float netWeightScale,
              const std::vector<Net*>& nets,
              const std::vector<Instance*>& placeInsts)
      : NetModel(maxFanout, netWeightScale, nets, placeInsts),
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