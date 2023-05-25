#include "BanyanGraphics.hpp"

#include "fassert.hpp"
#include "ord/OpenRoad.hh"
#include "utl/Logger.h"

namespace {

std::unordered_map<std::string, Polygon90Set> getOpenNetPolygons()
{
  std::unordered_map<std::string, Polygon90Set> ans;

  Polygon90Set s;
  // layer1
  s.insert(Rectangle(100, 100, 500, 500));
  s.insert(Rectangle(500, 200, 700, 300));
  s.insert(Rectangle(700, 100, 1000, 500));
  s.insert(Rectangle(800, 500, 900, 900));
  s.insert(Rectangle(400, 900, 1000, 1100));

  ans["net1"] = s;

  // layer2
  s.insert(Rectangle(1100, 1100, 1500, 1500));
  s.insert(Rectangle(1500, 1200, 1700, 1300));
  s.insert(Rectangle(1700, 1100, 1600, 1500));
  ans["net1"] = s;

  s.clear();
  s.insert(Rectangle(2100, 2100, 2500, 2500));
  s.insert(Rectangle(2500, 2200, 2700, 2300));
  s.insert(Rectangle(2700, 2100, 2600, 2500));
  ans["net2"] = s;

  return ans;
}

}  // namespace

namespace banyan {

BanyanGraphics::BanyanGraphics(utl::Logger* logger) : logger_(logger)
{
  gui::Gui::get()->registerRenderer(this);
}

void BanyanGraphics::status(const std::string& message)
{
  gui::Gui::get()->status(message);
}

/* static */
bool BanyanGraphics::guiActive()
{
  return gui::Gui::enabled();
}

void BanyanGraphics::drawObjects(gui::Painter& painter)
{
  painter.saveState();
  auto color = gui::Painter::Color(32, 216, 253);
  painter.setPenAndBrush(color);
  for (const auto& rect : polygon_rects_) {
    odb::Rect db_rect(xl(rect), yl(rect), xh(rect), yh(rect));
    painter.drawRect(db_rect);
  }
  painter.restoreState();
  color = gui::Painter::yellow;
  painter.setPenAndBrush(color);
  painter.setPenWidth(10 * painter.getPixelsPerDBU());
  for (const auto& line : lines_) {
    painter.drawLine(
        line.first.x(), line.first.y(), line.second.x(), line.second.y());
  }
}

void BanyanGraphics::drawPolygons(
    const std::unordered_map<std::string,
                             std::vector<bsp::polygon_90_data<int>>>&
        netPolygons,
    const std::unordered_map<std::string,
                             std::vector<std::pair<BsPoint, BsPoint>>>& netMst)
{
  polygon_rects_.clear();
  for (const auto& [net_name, polygon_vec] : netPolygons) {
    // It is much faster to decompose the set to rectangles once using boost
    // than trying to have Qt draw the polygons directly.
    for (const auto& p : polygon_vec) {
      bsp::get_rectangles(polygon_rects_, p);
    }
  }

  for (const auto& [net_name, netLines] : netMst) {
    lines_.insert(lines_.end(), netLines.begin(), netLines.end());
  }
  gui::Gui::get()->redraw();
  gui::Gui::get()->pause();
}

}  // namespace banyan