#pragma once

#include <boost/polygon/polygon.hpp>

#include "ByBox.hpp"
#include "gui/gui.h"

namespace bsp = boost::polygon;
using Rectangle = boost::polygon::rectangle_data<int>;
using Polygon90Set = boost::polygon::polygon_90_set_data<int>;

namespace utl {
class Logger;
}

namespace banyan {

// This class draws debugging graphics on the layout
class BanyanGraphics : public gui::Renderer
{
 public:
  BanyanGraphics(utl::Logger* logger);

  // Show a message in the status bar
  void status(const std::string& message);

  // Is the GUI being displayed (true) or are we in batch mode (false)
  static bool guiActive();

  // From Renderer API
  virtual void drawObjects(gui::Painter& painter) override;

  void drawPolygons(
      const std::unordered_map<std::string,
                               std::vector<bsp::polygon_90_data<int>>>&
          netPolygons,
      const std::unordered_map<std::string,
                               std::vector<std::pair<BsPoint, BsPoint>>>&
          netMst);

 private:
  utl::Logger* logger_;
  std::vector<Rectangle> polygon_rects_;
  std::vector<std::pair<BsPoint, BsPoint>> lines_;
};

}  // namespace banyan
