#pragma once

#include "odb/geom.h"

namespace gpl {

class IntRect
{
 public:
  IntRect() : rect_() {}
  IntRect(int x1, int y1, int x2, int y2) : rect_(x1, y1, x2, y2) {}
  int lx() const { return rect_.xMin(); }
  int ly() const { return rect_.yMin(); }
  int ux() const { return rect_.xMax(); }
  int uy() const { return rect_.yMax(); }

  int cx() const { return (lx() + ux()) / 2; }
  int cy() const { return (ly() + uy()) / 2; }
  int c(DIMENSION d) const { return d == X ? cx() : cy(); }

  int dx() const { return rect_.dx(); }
  int dy() const { return rect_.dy(); }

  // changing size and preserve center coordinates
  void resize(int dx, int dy);

  odb::Rect& rect() { return rect_; }

  const odb::Rect& rect() const { return rect_; }

 protected:
  odb::Rect rect_;
};

inline void IntRect::resize(int dx, int dy)
{
  const int lx = cx() - dx / 2;
  const int ly = cy() - dy / 2;
  const int ux = cx() + dx / 2;
  const int uy = cy() + dy / 2;
  rect_.reset(lx, ly, ux, uy);
}

inline int64_t getOverlapArea(const odb::Rect& a, const odb::Rect& b)
{
  return a.intersects(b) ? a.intersect(b).area() : 0;
}

}  // namespace gpl
