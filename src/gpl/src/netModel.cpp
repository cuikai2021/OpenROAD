#include "netModel.h"

#include "placerBase.h"

namespace gpl {

void NetModel::build()
{
  buildOn(X);
  buildOn(Y);
}

void NetModel::reset(DIMENSION dim)
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
}

// add a two pin connection into the graph, and update the matrix A and vector b
// , where w is the weight on the two pin connection
void NetModel::addTwoPinConn(Pin* pin1, Pin* pin2, float w, DIMENSION dim)
{
  auto& aList = matrixATriplets_[dim];
  auto& b = fixedInstForceVec_[dim];
  // both pins come from moveable instance
  if (pin1->isPlaceInstConnected() && pin2->isPlaceInstConnected()) {
    const int inst1 = pin1->instance()->extId();
    const int inst2 = pin2->instance()->extId();

    aList.push_back(T(inst1, inst1, w));
    aList.push_back(T(inst2, inst2, w));

    aList.push_back(T(inst1, inst2, -w));
    aList.push_back(T(inst2, inst1, -w));

    b(inst1) += w * (pin2->offsetC(dim) - pin1->offsetC(dim));
    b(inst2) += w * (pin1->offsetC(dim) - pin2->offsetC(dim));
  }
  // pin1 from IO port / pin2 from moveable instance
  else if (!pin1->isPlaceInstConnected() && pin2->isPlaceInstConnected()) {
    const int inst2 = pin2->instance()->extId();
    aList.push_back(T(inst2, inst2, w));
    b(inst2) += w * (pin1->c(dim) - pin2->offsetC(dim));
  }
  // pin2 from IO port / pin1 from moveable instance
  else if (!pin2->isPlaceInstConnected() && pin1->isPlaceInstConnected()) {
    const int inst1 = pin1->instance()->extId();
    aList.push_back(T(inst1, inst1, w));
    b(inst1) += w * (pin2->c(dim) - pin1->offsetC(dim));
  }
}

bool NetModel::isSkipped(Net* net) const
{
  // skip for small nets.
  if (net->pins().size() <= 1) {
    return true;
  }

  // escape long time cals on huge fanout.
  if (net->pins().size() >= maxFanout_) {
    return true;
  }
  return false;
}

}  // namespace gpl
