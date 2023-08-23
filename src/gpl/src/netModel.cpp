#include "netModel.h"

#include "envflag/envflag.hpp"
#include "linden/ignored_nets.hpp"
#include "gpl/placerBase.h"

IMPORT_F64_FLAG_WITH_DEFAULT(LINDEN_IGNORED_NETS_MAX_FANOUT);

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
  auto is_scan_chains_ignored_pin = [&](Pin* pin) {
    return pin->isITerm() && is_scan_chains_ignored_pin_(pin->dbITerm());
  };
  if (is_scan_chains_ignored_pin(pin0) || is_scan_chains_ignored_pin(pin1)) {
    return;
  }

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

  // movable but id == INT_MAX, means that one pin is not in the connected graph, don't add this. 
  for (auto cur_idx : {0, 1}) {
    const auto& cur = pins[cur_idx];
    if (cur.movable && cur.id == INT_MAX) {
      return;
    }
  }

  auto& aList = matrixATriplets_[dim];
  auto& b = fixedInstForceVec_[dim];
  for (auto cur_idx : {0, 1}) {
    const auto& cur = pins[cur_idx];
    if (cur.movable) {
      FASSERT(cur.id != INT_MAX);
      aList.push_back(T(cur.id, cur.id, w));
      const auto& ano = pins[cur_idx ^ 1];
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
  if (net->pins().size() == 1) {
    return true;
  }
  if (net->pins().size() >= LINDEN_IGNORED_NETS_MAX_FANOUT()) {
    return true;
  }
  return false;
}

double NetModel::getNetWeight(Net* net) const noexcept
{
  if (isSkipped(net)) {
    return 0;
  }
  auto w = ignored_nets_.get_net_weight(net->dbNet());
  return w * netWeightScale_ / (net->pins().size() - 1);
}

}  // namespace gpl
