#pragma once

#include <cassert>

namespace gpl {
namespace util {

template <typename T>
T bounded_value(T v, T min, T max)
{
  assert(min <= max);
  if (v < min) {
    return min;
  }
  if (v > max) {
    return max;
  }
  return v;
}

// https://stackoverflow.com/questions/33333363/built-in-mod-vs-custom-mod-function-improve-the-performance-of-modulus-op
inline int fastModulo(const int input, const int ceil)
{
  // apply the modulo operator only when needed
  // (i.e. when the input is greater than the ceiling)
  return input >= ceil ? input % ceil : input;
  // NB: the assumption here is that the numbers are positive
}

}  // end of namespace util
}  // end namespace gpl