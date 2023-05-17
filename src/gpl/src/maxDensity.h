
#pragma once

#include <vector>
#include <cstdint>

namespace maxDensity {

typedef std::pair<size_t, double> Id2Val;
inline size_t getId(const Id2Val& v)
{
  return v.first;
}

inline double getVal(const Id2Val& v)
{
  return v.second;
}

double slope(const std::vector<Id2Val>& X, size_t i, size_t j);

void max_density(std::pair<size_t, size_t>& result,
                 const std::vector<Id2Val>& X,
                 size_t L,
                 size_t U);

}  // namespace maxDensity