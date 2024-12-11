#include "netModelB2B.h"
#include "fassert.hpp"
#include <optional>
#include <iostream>
#include <vector>
#include <limits>
#include <array>
#include <algorithm>
#include <ranges>

using namespace std;

namespace linden::ip::ev {

// AX = b
// build matrix A and vector b from B2B modeling
void NetModelB2B::buildOn(DIMENSION dim)
{
    for (auto& x: nets_) {
        auto const& net = x.get();
        if (isSkipped(net)) {
            continue;
        }

        const double netWeight = getNetWeight(net);

        // foreach two pins in single nets.
        auto const& pins = net.pins();
        auto cmp = [=](const linden::db::PinRef& p0, const linden::db::PinRef& p1) {
            return p0.get().abs_location()[dim] < p1.get().abs_location()[dim];
        };
        auto min_pin_it = min_element(pins.begin(), pins.end(), cmp);
        auto const& min_pin = min_pin_it->get();
        auto max_pin_it = max_element(pins.begin(), pins.end(), cmp);
        auto const& max_pin = max_pin_it->get();
        array boundaries{&min_pin, &max_pin};
        for(size_t sz = pins.size(), i = 0; i < sz; ++i) {
            auto const& pin0 = pins[i].get();
            for(size_t j = i + 1; j < sz; ++j) {
                auto const& pin1 = pins[j].get();
                if (&pin0 == &pin1) {
                    continue;
                }
                if (ranges::find(boundaries, &pin0) == boundaries.end()
                    && ranges::find(boundaries, &pin1) == boundaries.end())
                {
                    continue;
                }
                float w = weight(pin0, pin1, netWeight, dim);
                addTwoPinConn(pin0, pin1, w, dim);
            }
        }
    }
}

// weight of a two pin connection
float NetModelB2B::weight(
    const linden::db::Pin& pin1,
    const linden::db::Pin& pin2,
    float netWeight,
    DIMENSION dim) const
{
    int64_t diff = abs(pin1.abs_location()[dim] - pin2.abs_location()[dim]);
    float w = 0;
    if (diff > minDiffLength_) {
        w = netWeight / diff;
    } else {
        w = netWeight / minDiffLength_;
    }
    return w;
}

}  // namespace linden::ip::ev
