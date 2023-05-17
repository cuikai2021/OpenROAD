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
  auto& data = fixedInstForceVec_[dim];
  data.resize(placeInstsNum());
  for (auto& inst : placeInsts()) {
    data(inst->extId()) = 0;
  }
}

// add a two pin connection into the graph, and update the matrix A and vector b
// , where w is the weight on the two pin connection
void NetModel::addTwoPinConn(Pin* pin0, Pin* pin1, float w, DIMENSION dim)
{
  struct PinInfo
  {
    Pin* pin;
    int id;
    bool movable;
    explicit PinInfo(Pin* p) : pin(p), id(-1), movable(false)
    {
      if (p) {
        movable = p->isPlaceInstConnected();
        if (p->instance()) {
          id = p->instance()->extId();
        }
      }
    }
  } pins[2] = {PinInfo(pin0), PinInfo(pin1)};

  auto& aList = matrixATriplets_[dim];
  auto& b = fixedInstForceVec_[dim];
  for (auto cur_idx : {0, 1}) {
    const auto& cur = pins[cur_idx];
    if (cur.movable) {
      aList.push_back(T(cur.id, cur.id, w));
      const auto& ano = pins[(cur_idx + 1) % 2];
      if (ano.movable) {
        aList.push_back(T(cur.id, ano.id, -w));
        b(cur.id) += w * (ano.pin->offsetC(dim) - cur.pin->offsetC(dim));
      } else {
        b(cur.id) += w * (ano.pin->c(dim) - cur.pin->offsetC(dim));
      }
    }
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
