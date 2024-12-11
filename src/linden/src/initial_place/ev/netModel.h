#pragma once

#include "dimension.hpp"
#include "../../util/ignored_nets.hpp"
#include "linden/db/db.hpp"
#include "linden/db/cell.hpp"
#include "linden/db/pin.hpp"
#include "linden/db/net.hpp"
#include <Eigen/SparseCore>
#include <deque>
#include <vector>
#include <unordered_map>

namespace linden::ip::ev {

// Net Modeling which converts hyper graph into two-pin connections
class NetModel
{
public:
    typedef Eigen::Triplet<float> T;
    typedef std::vector<T> Triplets;

    virtual ~NetModel() =default;

    const Triplets& getMatrixATriplets(DIMENSION dim) {
        return matrixATriplets_[dim];
    }

    const Eigen::VectorXf& getVectorB(DIMENSION dim) {
        return fixedInstForceVec_[dim];
    }

protected:
    explicit NetModel(
        float netWeightScale,
        const std::deque<linden::db::ConstNetRef>& nets,
        const std::vector<linden::db::Cell*>& placeInsts,
        const std::unordered_map<const linden::db::Cell*, size_t>& placeable_insts_indices);

    virtual float weight(
        const linden::db::Pin& pin1,
        const linden::db::Pin& pin2,
        float netWeight,
        DIMENSION dim) const = 0;

    void addTwoPinConn(
        const linden::db::Pin&,
        const linden::db::Pin&,
        float w,
        DIMENSION dim);
    bool isSkipped(const linden::db::Net&) const;
    double getNetWeight(const linden::db::Net&) const noexcept;

    float netWeightScale_;
    const std::deque<linden::db::ConstNetRef>& nets_;
    const std::vector<linden::db::Cell*>& placeInsts_;// moveable instances
    const std::unordered_map<const linden::db::Cell*, size_t>& placeable_insts_indices_;
    IgnoredNets ignored_nets_;

    Triplets matrixATriplets_[DIMENSION_CNT];// matrix A
    Eigen::VectorXf fixedInstForceVec_[DIMENSION_CNT];// vector b
};

}  // namespace linden::ip::ev
