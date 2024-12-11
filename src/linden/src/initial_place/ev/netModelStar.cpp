#include "netModelStar.h"
#include "odb/db.h"
#include "odb/dbTypes.h"
#include "fassert.hpp"
#include <iostream>

namespace linden::ip::ev {

// AX = b
// build matrix A and vector b from Star modeling
void NetModelStar::buildOn(DIMENSION dim)
{
    for (auto& x: nets_) {
        auto const& net = x.get();
        if (isSkipped(net)) {
            continue;
        }

        const double netWeight = getNetWeight(net);

        const auto& pin1 = getSourcePin(net);
        for (const auto& p2 : net.pins()) {
            const auto& pin2 = p2.get();
            if (&pin1 == &pin2) {
                continue;
            }
            float w = weight(pin1, pin2, netWeight, dim);
            addTwoPinConn(pin1, pin2, w, dim);
        }
    }
}

bool NetModelStar::isSourcePin(const linden::db::Pin& pin) const
{
    if (pin.cell()) {
        auto o_iterm = reinterpret_cast<odb::dbITerm*>(pin.odb_term());
        return o_iterm->getIoType() != odb::dbIoType::INPUT;
    } else {
        auto o_bterm = reinterpret_cast<odb::dbBTerm*>(pin.odb_term());
        return o_bterm->getIoType() != odb::dbIoType::OUTPUT;
    }
}

const linden::db::Pin& NetModelStar::getSourcePin(const linden::db::Net& net) const
{
    for (auto& pin: net.pins()) {
        if (isSourcePin(pin)) {
            return pin;
        }
    }
    FASSERT(false)
        .what("net must have a source pin");
    return net.pins()[0];
}

// weight of a two pin connection
float NetModelStar::weight(
    const linden::db::Pin& pin1,
    const linden::db::Pin& pin2,
    float netWeight,
    DIMENSION dim) const
{
    return netWeight;
}

}  // namespace linden::ip::ev
