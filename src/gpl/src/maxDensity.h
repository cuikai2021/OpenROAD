
#pragma once

#include <vector>
#include <cstdint>

namespace maxDensity {

typedef std::pair<std::size_t, double> Id2Val;
inline std::size_t getId(const Id2Val& v)
{
  return v.first;
}

inline double getVal(const Id2Val& v)
{
  return v.second;
}

double slope(const std::vector<Id2Val>& X, std::size_t i, std::size_t j);

void max_density(std::pair<std::size_t, std::size_t>& result,
                 const std::vector<Id2Val>& X,
                 std::size_t L,
                 std::size_t U);

}  // namespace maxDensity