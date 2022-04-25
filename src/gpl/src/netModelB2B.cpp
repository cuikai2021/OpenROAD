#include "netModelB2B.h"

#include "placerBase.h"

#include <iostream>

namespace gpl {

// AX = b
// create matrix A and vector b from B2B modeling
void NetModelB2B::buildOn(DIMENSION dim)
{
  // listX and listY is a temporary vector that have tuples, (idx1, idx2, val)
  auto& aList = matrixATriplets_[dim];
  aList.clear();
  aList.reserve(2*placeInstsNum());

  // initialize vector
  fixedInstForceVec_[dim].resize(placeInstsNum());
  for (auto& inst : placeInsts()) {
    int idx = inst->extId();
    fixedInstForceVec_[dim](idx) = 0;
  }

  // for each net
  for (auto& net : nets()) {
    // skip for small nets.
    if (net->pins().size() <= 1) {
      continue;
    }

    // escape long time cals on huge fanout.
    //
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
        if (pin1->isMinPin(dim) || pin1->isMaxPin(dim) || pin2->isMinPin(dim)
            || pin2->isMaxPin(dim)) {
          int diff = abs(pin1->c(dim) - pin2->c(dim));
          float weight = 0;
          if (diff > minDiffLength_) {
            weight = netWeight / diff;
          } else {
            weight = netWeight / minDiffLength_;
          }

          // both pin cames from instance
          if (pin1->isPlaceInstConnected() && pin2->isPlaceInstConnected()) {
            const int inst1 = pin1->instance()->extId();
            const int inst2 = pin2->instance()->extId();
            // cout << "inst: " << inst1 << " " << inst2 << endl;

            aList.push_back(T(inst1, inst1, weight));
            aList.push_back(T(inst2, inst2, weight));

            aList.push_back(T(inst1, inst2, -weight));
            aList.push_back(T(inst2, inst1, -weight));

            fixedInstForceVec_[dim](inst1)
                += weight * (pin2->offsetC(dim) - pin1->offsetC(dim));

            fixedInstForceVec_[dim](inst2)
                += weight * (pin1->offsetC(dim) - pin2->offsetC(dim));
          }
          // pin1 from IO port / pin2 from Instance
          else if (!pin1->isPlaceInstConnected()
                   && pin2->isPlaceInstConnected()) {
            const int inst2 = pin2->instance()->extId();
            // cout << "inst2: " << inst2 << endl;
            aList.push_back(T(inst2, inst2, weight));
            fixedInstForceVec_[dim](inst2)
                += weight * (pin1->c(dim) - pin2->offsetC(dim));
          }
          // pin1 from Instance / pin2 from IO port
          else if (pin1->isPlaceInstConnected()
                   && !pin2->isPlaceInstConnected()) {
            const int inst1 = pin1->instance()->extId();
            // cout << "inst1: " << inst1 << endl;
            aList.push_back(T(inst1, inst1, weight));
            fixedInstForceVec_[dim](inst1)
                += weight * (pin2->c(dim) - pin1->offsetC(dim));
          }
        }
      }
    }
  }
}  

}  // namespace gpl