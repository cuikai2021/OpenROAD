#include "maxDensity.h"

#include <cassert>
#include <iostream>

namespace maxDensity {

/* For a sorted sequence X = <x1,x2, ... , xn> of real numbers,
* a segment of X is a subsequence <xi, ..., xj> for i <= j.
* The length of the segment is j - i + 1 and its density is
* u_i_j = (j - i + 1) / (Xj - Xi)
*
* In the maximum-density segment problem, a sequnce X and two size_tegers, L and
U,
* with 1 <= L <= U <= n, are given as input, and we want to find a maximum
possible
* density segment of X with length at least L and at most U.
*
* This algorithm is implemented from paper <<Linear-time algorithm for finding a
maximum-density segment of a sequence>> by Sung Kwon Kim, which solves this
problem in O(n) time by geometric algorithms.
*/

struct MDPoint
{
  double x;
  double y;

  MDPoint(double x_, double y_) : x(x_), y(y_) {}
};

struct MDLine
{
  MDPoint point;
  double slope;

  MDLine(const MDPoint& p_, double s_) : point(p_), slope(s_) {}

  bool isPointAbove(
      const MDPoint& p);  // check whether the point is above this line

  static double getSlope(const MDPoint& p0, const MDPoint& p1)
  {
    return (p1.y - p0.y) / (p1.x - p0.x);
  }
};

bool MDLine::isPointAbove(const MDPoint& p)
{
  if (p.x > this->point.x) {
    return getSlope(this->point, p) > this->slope;
  } else {
    return getSlope(this->point, p) < this->slope;
  }
}

double slope(const std::vector<Id2Val>& X, size_t i, size_t j)
{
  return (j - i) / (getVal(X[j]) - getVal(X[i]));
}

void max_density(std::pair<size_t, size_t>& result,
                 const std::vector<Id2Val>& X,
                 size_t L,
                 size_t U)
{
#ifndef NDEBUG
  for (size_t i = 1; i < X.size(); ++i) {
    assert(getVal(X[i]) >= getVal(X[i - 1]));
  }
#endif
  assert(L >= 2);
  assert(U >= L);
  result.first = 0;
  result.second = L - 1;
  MDLine line(MDPoint(getVal(X[result.first]), result.first),
              slope(X, result.first, result.second));
#if 0
#define DEBUG_PRINT_MD_RESULT                                     \
  printf(                                                         \
      "__LINE__=%d: line.slope=%f, max_density.result=(%d, %d), " \
      "line.point=(%f, %f)\n",                                    \
      __LINE__,                                                   \
      line.slope,                                                 \
      result.first,                                               \
      result.second,                                              \
      line.point.x,                                               \
      line.point.y);
#else
#define DEBUG_PRINT_MD_RESULT ((void) 0);
#endif

  for (size_t j = L; j < X.size(); ++j) {
    // L <= j - i + 1 <= U
    auto imin = std::max((j + 1) - U, (size_t) 0);
    auto imax = (j + 1) - L;
#if 0  // the O(n^2) complexity implementation, which is to check the
       // correctness of this algorithm.
    for (auto i = imin; i <= imax; ++i) {
      auto sl = slope(X, i, j);
      if (sl > line.slope) {
        line.slope = sl;
        line.point = MDPoint(getVal(X[i]), i);
        result.first = i;
        result.second = j;
        DEBUG_PRINT_MD_RESULT
      }
    }
    continue;
#endif
    auto y = imax;
    MDPoint p_y(getVal(X[y]), y);
    MDPoint p_j(getVal(X[j]), j);
    if (line.isPointAbove(p_y)) {
      if (line.isPointAbove(p_j)) {
        size_t alpha = line.point.y;
        for (auto i = std::max(imin, alpha); i <= imax; ++i) {
          auto sl = slope(X, i, j);
          if (sl > line.slope) {
            line.slope = sl;
            line.point = MDPoint(getVal(X[i]), i);
            result.first = i;
            result.second = j;
            DEBUG_PRINT_MD_RESULT
          }
        }
      } else {
        // In this subcase we have µ' <= µ. So, we do not need to compute µ.
        // Both α and l remain unchanged
      }
    } else {
      // py is on or below line
      // In this case α will not be on Hj+1. Let l' be the line that passes
      // through py and is parallel to l
      line = MDLine(p_y, line.slope);
      if (line.isPointAbove(p_j)) {
        line = MDLine(p_y, slope(X, y, j));
        result.first = y;
        result.second = j;
        DEBUG_PRINT_MD_RESULT
      } else {
        // In this subcase we have µ_j <= µ.
        // Set line.slope no change and line.point = py;
        line.point = p_y;
        DEBUG_PRINT_MD_RESULT
      }
    }
  }
  DEBUG_PRINT_MD_RESULT
}

}  // namespace maxDensity