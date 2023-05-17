#include "netModelB2B.h"

#include <iostream>

#include "placerBase.h"

namespace gpl {

// AX = b
// build matrix A and vector b from B2B modeling
void NetModelB2B::buildOn(DIMENSION dim)
{
  reset(dim);
  for (auto& net : nets()) {
    if (isSkipped(net)) {
      continue;
    }

    float netWeight = netWeightScale_ / (net->pins().size() - 1);

    // foreach two pins in single nets.
    auto& pins = net->pins();
    for (int pinIdx1 = 1; pinIdx1 < pins.size(); ++pinIdx1) {
      Pin* pin1 = pins[pinIdx1];
      for (int pinIdx2 = 0; pinIdx2 < pinIdx1; ++pinIdx2) {
        Pin* pin2 = pins[pinIdx2];

        // no need to fill in when instance is same
        if (pin1->instance() == pin2->instance()) {
          continue;
        }

        // B2B modeling on min/maxX pins.
        if (isBoundary(pin1, dim) || isBoundary(pin2, dim)) {
          float w = weight(pin1, pin2, netWeight, dim);
          addTwoPinConn(pin1, pin2, w, dim);
        }
      }
    }
  }
}

// weight of a two pin connection
float NetModelB2B::weight(Pin* pin1,
                          Pin* pin2,
                          float netWeight,
                          DIMENSION dim) const
{
  assert(pin1 && pin2);
  int diff = abs(pin1->c(dim) - pin2->c(dim));
  float w = 0;
  if (diff > minDiffLength_) {
    w = netWeight / diff;
  } else {
    w = netWeight / minDiffLength_;
  }
  return w;
}

bool NetModelB2B::isBoundary(Pin* pin, DIMENSION dim) const
{
  return pin->isMinPin(dim) || pin->isMaxPin(dim);
}

}  // namespace gpl