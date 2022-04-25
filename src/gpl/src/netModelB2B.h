#pragma once

#include "netModel.h"

namespace gpl {

// Bound2Bound Net Model
class NetModelB2B : public NetModel
{
 public:
  NetModelB2B(size_t minDiffLength,
              std::size_t maxFanout,
              std::size_t netWeightScale,
              const std::vector<Net*>& nets,
              const std::vector<Instance*>& placeInsts)
      : NetModel(maxFanout, netWeightScale, nets, placeInsts),
        minDiffLength_(minDiffLength)
  {
    build();
  }

 protected:
  virtual void buildOn(DIMENSION dim);

 private:
  size_t minDiffLength_;
};

}  // namespace gpl