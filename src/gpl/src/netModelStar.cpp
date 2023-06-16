#include "netModelStar.h"

#include <iostream>

#include "odb/db.h"
#include "odb/dbTypes.h"
#include "placerBase.h"

namespace gpl {

// AX = b
// build matrix A and vector b from Star modeling
void NetModelStar::buildOn(DIMENSION dim)
{
  reset(dim);
  for (auto& net : nets()) {
    if (isSkipped(net)) {
      continue;
    }

    const double netWeight = getNetWeight(net);

    auto pin1 = getSourcePin(net);
    for (auto& pin2 : net->pins()) {
      // no need to fill in when instance is same
      if (pin1->instance() == pin2->instance()) {
        continue;
      }
      float w = weight(pin1, pin2, netWeight, dim);
      addTwoPinConn(pin1, pin2, w, dim);
    }
  }
}

bool NetModelStar::isSourcePin(Pin* pin) const
{
  if (pin->isITerm()) {
    return pin->dbITerm()->getIoType() != odb::dbIoType::INPUT;
  } else {
    return pin->dbBTerm()->getIoType() != odb::dbIoType::OUTPUT;
  }
}

Pin* NetModelStar::getSourcePin(Net* net) const
{
  auto pins = net->pins();
  for (auto& pin : pins) {
    if (isSourcePin(pin)) {
      return pin;
    }
  }
  assert(0 && "net must have a source pin");
  return nullptr;
}

// weight of a two pin connection
float NetModelStar::weight(Pin* pin1,
                           Pin* pin2,
                           float netWeight,
                           DIMENSION dim) const
{
  assert(pin1 && pin2);
  auto w = netWeight;
  return w;
}

}  // namespace gpl