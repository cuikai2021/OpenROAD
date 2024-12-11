#include "netModel.h"
#include "linden/db/pin.hpp"
#include "envflag/envflag.hpp"
#include "fassert.hpp"
#include <optional>

IMPORT_F64_FLAG_WITH_DEFAULT(LINDEN_IGNORED_NETS_MAX_FANOUT);

using namespace std;

namespace linden::ip::ev {

NetModel::NetModel(
    float netWeightScale,
    const std::deque<linden::db::ConstNetRef>& nets,
    const std::vector<linden::db::Cell*>& placeInsts,
    const std::unordered_map<const linden::db::Cell*, size_t>& placeable_insts_indices)
:   netWeightScale_(netWeightScale),
    nets_(nets),
    placeInsts_(placeInsts),
    placeable_insts_indices_(placeable_insts_indices)
{
    for(auto dim: {X, Y}) {
        auto& aList = matrixATriplets_[dim];
        aList.clear();
        aList.reserve(2 * placeInsts_.size());

        // initialize vector
        auto& data = fixedInstForceVec_[dim];
        data.resize(placeInsts_.size());
    }
}

// add a two pin connection into the graph, and update the matrix A and vector b
// , where w is the weight on the two pin connection
void NetModel::addTwoPinConn(
    const linden::db::Pin& pin0,
    const linden::db::Pin& pin1,
    float w,
    DIMENSION dim)
{

    struct PinInfo
    {
        const linden::db::Pin& pin;
        bool movable;
        size_t index;

        explicit PinInfo(const linden::db::Pin& pin)
        :   pin(pin),
            movable(false),
            index(0)
        {}

        static optional<PinInfo> build(
            const unordered_map<const linden::db::Cell*, size_t>& rev_idx,
            const linden::db::Pin& p)
        {
            auto c = p.cell();
            if (!c) {
                return nullopt;
            }
            auto& cell = c->get();
            auto it = rev_idx.find(&cell);
            if (it == rev_idx.end()) {
                return nullopt;
            }
            PinInfo res(p);
            res.movable = !cell.is_virtual() && cell.moveable();
            res.index = it->second;
            return res;
        }
    };
    auto pi0 = PinInfo::build(placeable_insts_indices_, pin0);
    if (!pi0) {
        return;
    }
    auto pi1 = PinInfo::build(placeable_insts_indices_, pin1);
    if (!pi1) {
        return;
    }
    PinInfo pins[2] = {*pi0, *pi1};

    auto& aList = matrixATriplets_[dim];
    auto& b = fixedInstForceVec_[dim];
    for (auto cur_idx : {0, 1}) {
        const auto& cur = pins[cur_idx];
        if (cur.movable) {
            aList.push_back(T(cur.index, cur.index, w));
            const auto& ano = pins[cur_idx ^ 1];
            if (ano.movable) {
                aList.push_back(T(cur.index, ano.index, -w));
                b(cur.index) += w * (ano.pin.offset()[dim] - cur.pin.offset()[dim]);
            } else {
                b(cur.index) += w * (ano.pin.abs_location()[dim] - cur.pin.offset()[dim]);
            }
        }
    }
}

bool NetModel::isSkipped(const linden::db::Net& net) const
{
    if (net.pins().size() <= 1) {
        return true;
    }
    if (net.pins().size() >= LINDEN_IGNORED_NETS_MAX_FANOUT()) {
        return true;
    }
    return false;
}

double NetModel::getNetWeight(const linden::db::Net& net) const noexcept
{
    auto o_net = reinterpret_cast<odb::dbNet*>(net.odb_net());
    auto w = ignored_nets_.get_net_weight(o_net);
    return w * netWeightScale_ / (net.pins().size() - 1);
}

}  // namespace linden::ip::ev
