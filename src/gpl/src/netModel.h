#pragma once

#include <Eigen/SparseCore>

#include "dimension.h"
#include "linden/ignored_nets.hpp"

namespace gpl {
class Instance;
class Pin;
class Net;

// Net Modeling which converts hyper graph into two-pin connections
class NetModel
{
 public:
  typedef Eigen::Triplet<float> T;
  typedef std::vector<T> Triplets;

  NetModel(float netWeightScale,
           const IsScanChainsIgnoredPin& is_scan_chains_ignored_pin,
           const IgnoredNets& ignored_nets,
           const std::vector<Net*>& nets,
           const std::vector<Instance*>& placeInsts)
      : netWeightScale_(netWeightScale),
        is_scan_chains_ignored_pin_(is_scan_chains_ignored_pin),
        ignored_nets_(ignored_nets),
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
  size_t placeInstsNum() const { return placeInsts().size(); }

 protected:
  void reset(DIMENSION dim);

  void build();

  virtual void buildOn(DIMENSION dim) = 0;
  virtual float weight(Pin* pin1,
                       Pin* pin2,
                       float netWeight,
                       DIMENSION dim) const = 0;

  void addTwoPinConn(Pin* pin0, Pin* pin1, float w, DIMENSION dim);
  bool isSkipped(Net* net) const;
  double getNetWeight(Net* net) const noexcept;

  float netWeightScale_;

  const IsScanChainsIgnoredPin& is_scan_chains_ignored_pin_;
  const IgnoredNets& ignored_nets_;
  const std::vector<Net*>& nets_;
  const std::vector<Instance*>& placeInsts_;          // moveable instances
  Triplets matrixATriplets_[DIMENSION_CNT];           // matrix A
  Eigen::VectorXf fixedInstForceVec_[DIMENSION_CNT];  // vector b
};

}  // namespace gpl