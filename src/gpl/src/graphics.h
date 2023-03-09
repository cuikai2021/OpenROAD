#pragma once

#include <memory>

#include "gui/gui.h"

namespace utl {
class Logger;
}

namespace linden {
namespace db {
class Database;
}

namespace rdp {
class CongestionMap;
}

}  // namespace linden

namespace gpl {

class InitialPlace;
class NesterovBase;
class NesterovPlace;
class PlacerBase;
class GCell;

// This class draws debugging graphics on the layout
class Graphics : public gui::Renderer
{
 public:
  // Debug InitialPlace
  Graphics(utl::Logger* logger, std::shared_ptr<PlacerBase> pb);

  // Debug NesterovPlace
  Graphics(utl::Logger* logger,
           NesterovPlace* np,
           std::shared_ptr<PlacerBase> pb,
           std::shared_ptr<NesterovBase> nb,
           bool draw_bins);

  // Debug linden::gpl::Placer
  Graphics(utl::Logger* logger,
           std::shared_ptr<linden::db::Database> linden_db_,
           std::shared_ptr<linden::rdp::CongestionMap> banyan_cmap_result,
           std::shared_ptr<PlacerBase> pb);

  // Draw the graphics; optionally pausing afterwards
  void cellPlot(bool pause = false);

  // Show a message in the status bar
  void status(const std::string& message);

  // From Renderer API
  virtual void drawObjects(gui::Painter& painter) override;
  virtual gui::SelectionSet select(odb::dbTechLayer* layer,
                                   const odb::Rect& region) override;

  // Is the GUI being displayed (true) or are we in batch mode (false)
  static bool guiActive();

  // highlight focusing names from lindengp
  void highlightFocus(const std::vector<std::vector<std::string>>& focus_names);

  const char* getDisplayControlGroupName() override;

 private:
  std::shared_ptr<PlacerBase> pb_;
  std::shared_ptr<NesterovBase> nb_;
  NesterovPlace* np_;
  std::shared_ptr<linden::db::Database> linden_db_;
  std::shared_ptr<linden::rdp::CongestionMap> banyan_cmap_result_;
  GCell* selected_;
  bool draw_bins_;
  utl::Logger* logger_;
  enum Label
  {
    LINDEN,
    CMAP_H,
    CMAP_V,
    EXPANDER,
    MACRO
  };
  struct LabeledRect
  {
    odb::Rect rect;
    gui::Painter::Color color;
    Label label;
  };
  std::vector<LabeledRect> labeledRects_;

  void drawLinden(gui::Painter& painter);
  void drawBanyanCMap(gui::Painter& painter);

  void drawNesterov(gui::Painter& painter);
  void drawInitial(gui::Painter& painter);
  void drawBounds(gui::Painter& painter);
  void reportSelected();

  static std::string label2name(const Label& l);
  bool isLablelSelected(const Label& l);
  void addRectToLabelSet(const odb::Rect& rect,
                         const gui::Painter::Color& color,
                         const Label& label);
  void drawRectsInLabels(gui::Painter& painter);
};

}  // namespace gpl
