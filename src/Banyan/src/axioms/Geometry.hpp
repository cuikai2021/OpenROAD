#pragma once

#include <assert.h>
#include <inttypes.h>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/adapted/boost_polygon.hpp>
#include <boost/polygon/polygon.hpp>

#include "odb/db.h"

namespace banyan {
namespace gtl = boost::polygon;
using namespace boost::polygon::operators;

using Rectangle = gtl::rectangle_data<int64_t>;
using Polygon90 = gtl::polygon_90_with_holes_data<int64_t>;
using Polygon90Set = gtl::polygon_90_set_data<int64_t>;

inline uint64_t getBoxDx(const Rectangle& box) noexcept
{
  return gtl::delta(box, gtl::orientation_2d_enum::HORIZONTAL);
}

inline uint64_t getBoxDy(const Rectangle& box) noexcept
{
  return gtl::delta(box, gtl::orientation_2d_enum::VERTICAL);
}

inline uint64_t getBoxCenterToLeftEdge(const Rectangle& box) noexcept
{
  auto boxLL = gtl::ll(box);
  return std::abs(boxLL.x());
}

inline uint64_t getBoxCenterToRightEdge(const Rectangle& box) noexcept
{
  auto boxUR = gtl::ur(box);
  return std::abs(boxUR.x());
}

inline uint64_t getBoxCenterToTopEdge(const Rectangle& box) noexcept
{
  auto boxUR = gtl::ur(box);
  return std::abs(boxUR.y());
}

inline uint64_t getBoxCenterToBottomEdge(const Rectangle& box) noexcept
{
  auto boxLL = gtl::ll(box);
  return std::abs(boxLL.y());
}

}  // namespace banyan