#pragma once

#include "netModel.h"

namespace linden::ip::ev {

// Star Net Model
class NetModelStar : public NetModel
{
public:
    NetModelStar(
        float netWeightScale,
        const std::deque<linden::db::ConstNetRef>& nets,
        const std::vector<linden::db::Cell*>& placeInsts,
        const std::unordered_map<const linden::db::Cell*, size_t>& placeable_insts_indices)
    :   NetModel(netWeightScale, nets, placeInsts, placeable_insts_indices)
    {
        buildOn(X);
        buildOn(Y);
    }

protected:
    void buildOn(DIMENSION dim);
    float weight(
        const linden::db::Pin& pin1,
        const linden::db::Pin& pin2,
        float netWeight,
        DIMENSION dim) const override;

    bool isSourcePin(const linden::db::Pin& pin) const;
    const linden::db::Pin& getSourcePin(const linden::db::Net& net) const;
};

}  // namespace linden::ip::ev