
#pragma once

#include <vector>

namespace maxDensity {

typedef std::pair<int, double> Id2Val;
int getId(const Id2Val& v);
double getVal(const Id2Val& v);
double slope(const std::vector<Id2Val>& X, int i, int j);

void max_density(std::pair<int, int>& result,
                        const std::vector<Id2Val>& X,
                        int L,
                        int U);
                        
} // end of namespace gpl_util