#pragma once

#include <Eigen/SparseCore>

#include "dimension.h"

namespace gpl {
class Instance;
class Net;

// Net Modeling which converts hyper graph into two-pin connections
class NetModel
{
 public:
  typedef Eigen::Triplet<float> T;
  typedef std::vector<T> Triplets;

  NetModel(std::size_t maxFanout,
           std::size_t netWeightScale,
           const std::vector<Net*>& nets,
           const std::vector<Instance*>& placeInsts)
      : maxFanout_(maxFanout),
        netWeightScale_(netWeightScale),
        nets_(nets),
        placeInsts_(placeInsts)
  {
  }

  const Triplets& getMatrixATriplets(DIMENSION dim)
  {
    return matrixATriplets_[dim];
  }
  const Eigen::VectorXf& getVectorB(DIMENSION dim)
  {
    return fixedInstForceVec_[dim];
  }

  const std::vector<Net*>& nets() const { return nets_; }
  const std::vector<Instance*>& placeInsts() const { return placeInsts_; }
  std::size_t placeInstsNum() const { return placeInsts().size(); }

 protected:
  void build();
  virtual void buildOn(DIMENSION dim) = 0;

  std::size_t maxFanout_;
  std::size_t netWeightScale_;
  const std::vector<Net*>& nets_;
  const std::vector<Instance*>& placeInsts_;                 // moveable instances
  Triplets matrixATriplets_[DIMENSION_CNT];           // matrix A
  Eigen::VectorXf fixedInstForceVec_[DIMENSION_CNT];  // vector b
};

}  // namespace gpl