#pragma once

#include <cassert>

namespace gpl {
namespace util {

template<typename T>
T bounded_value(T v, T min, T max)
{
  assert(min < max);
  if (v < min) {
    return min;
  }
  if (v > max) {
    return max;
  }
  return v;
}

}  // end of namespace util
}  // end namespace gpl