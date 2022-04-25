#include "netModelB2B.h"

#include <iostream>

#include "placerBase.h"

namespace gpl {

// AX = b
// create matrix A and vector b from B2B modeling
void NetModelB2B::buildOn(DIMENSION dim)
{
  // triplets of (idx1, idx2, val)
  auto& aList = matrixATriplets_[dim];
  aList.clear();
  aList.reserve(2 * placeInstsNum());

  // initialize vector
  fixedInstForceVec_[dim].resize(placeInstsNum());
  for (auto& inst : placeInsts()) {
    int idx = inst->extId();
    fixedInstForceVec_[dim](idx) = 0;
  }

  for (auto& net : nets()) {
    // skip for small nets.
    if (net->pins().size() <= 1) {
      continue;
    }

    // escape long time cals on huge fanout.
    if (net->pins().size() >= maxFanout_) {
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

          // both pin cames from instance
          if (pin1->isPlaceInstConnected() && pin2->isPlaceInstConnected()) {
            const int inst1 = pin1->instance()->extId();
            const int inst2 = pin2->instance()->extId();

            aList.push_back(T(inst1, inst1, w));
            aList.push_back(T(inst2, inst2, w));

            aList.push_back(T(inst1, inst2, -w));
            aList.push_back(T(inst2, inst1, -w));

            fixedInstForceVec_[dim](inst1)
                += w * (pin2->offsetC(dim) - pin1->offsetC(dim));

            fixedInstForceVec_[dim](inst2)
                += w * (pin1->offsetC(dim) - pin2->offsetC(dim));
          }
          // pin1 from IO port / pin2 from Instance
          else if (!pin1->isPlaceInstConnected()
                   && pin2->isPlaceInstConnected()) {
            const int inst2 = pin2->instance()->extId();
            aList.push_back(T(inst2, inst2, w));
            fixedInstForceVec_[dim](inst2)
                += w * (pin1->c(dim) - pin2->offsetC(dim));
          }
          // pin1 from Instance / pin2 from IO port
          else if (pin1->isPlaceInstConnected()
                   && !pin2->isPlaceInstConnected()) {
            const int inst1 = pin1->instance()->extId();
            aList.push_back(T(inst1, inst1, w));
            fixedInstForceVec_[dim](inst1)
                += w * (pin2->c(dim) - pin1->offsetC(dim));
          }
        }
      }
    }
  }
}

// weight of a two pin connection
float NetModelB2B::weight(Pin* pin1, Pin* pin2, float netWeight, DIMENSION dim)
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

bool NetModelB2B::isBoundary(Pin* pin, DIMENSION dim) {
  return pin->isMinPin(dim) || pin->isMaxPin(dim);
}

}  // namespace gpl