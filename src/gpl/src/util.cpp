#include "util.h"

#include <cassert>

namespace gpl {
namespace util {

float bounded_value(float f, float min, float max)
{
  assert(min < max);
  if (f < min) {
    return min;
  }
  if (f > max) {
    return max;
  }
  return f;
}

}  // namespace util
}  // end namespace gpl