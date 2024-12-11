#pragma once

#include "netModel.h"

namespace linden::ip::ev {

// Bound2Bound Net Model
class NetModelB2B : public NetModel
{
public:
    NetModelB2B(
        size_t minDiffLength,
        float netWeightScale,
        const std::deque<linden::db::ConstNetRef>& nets,
        const std::vector<linden::db::Cell*>& placeInsts,
        const std::unordered_map<const linden::db::Cell*, size_t>& placeable_insts_indices)
    :   NetModel(netWeightScale, nets, placeInsts, placeable_insts_indices),
        minDiffLength_(minDiffLength)
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

private:
    size_t minDiffLength_;
};

}  // namespace linden::ip::ev
