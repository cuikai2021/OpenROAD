#include "graphics.h"

#include <algorithm>
#include <cstdio>
#include <limits>

#include "nesterovBase.h"
#include "nesterovPlace.h"
#include "placerBase.h"
#include "linden/db/db.hpp"
#include "utl/Logger.h"

namespace gpl {

Graphics::Graphics(utl::Logger* logger, std::shared_ptr<PlacerBase> pb)
    : pb_(pb),
      nb_(),
      np_(nullptr),
      linden_db_(nullptr),
      selected_(nullptr),
      draw_bins_(false),
      logger_(logger)
{
  gui::Gui::get()->registerRenderer(this);
}

Graphics::Graphics(utl::Logger* logger,
                   NesterovPlace* np,
                   std::shared_ptr<PlacerBase> pb,
                   std::shared_ptr<NesterovBase> nb,
                   bool draw_bins)
    : pb_(pb),
      nb_(nb),
      np_(np),
      linden_db_(nullptr),
      selected_(nullptr),
      draw_bins_(draw_bins),
      logger_(logger)
{
  gui::Gui::get()->registerRenderer(this);
}

Graphics::Graphics(utl::Logger* logger,
                   std::shared_ptr<linden::db::Database> linden_db,
                   std::shared_ptr<PlacerBase> pb)
    : pb_(pb),
      nb_(),
      np_(nullptr),
      linden_db_(linden_db),
      selected_(nullptr),
      draw_bins_(false),
      logger_(logger)
{
  gui::Gui::get()->registerRenderer(this);
}

void Graphics::drawBounds(gui::Painter& painter)
{
  // draw core bounds
  auto& die = pb_->die();
  painter.setPen(gui::Painter::yellow, /* cosmetic */ true);
  painter.drawLine(die.coreLx(), die.coreLy(), die.coreUx(), die.coreLy());
  painter.drawLine(die.coreUx(), die.coreLy(), die.coreUx(), die.coreUy());
  painter.drawLine(die.coreUx(), die.coreUy(), die.coreLx(), die.coreUy());
  painter.drawLine(die.coreLx(), die.coreUy(), die.coreLx(), die.coreLy());
}

void Graphics::drawInitial(gui::Painter& painter)
{
  drawBounds(painter);

  painter.setPen(gui::Painter::white, /* cosmetic */ true);
  for (auto& inst : pb_->placeInsts()) {
    int lx = inst->lx();
    int ly = inst->ly();
    int ux = inst->ux();
    int uy = inst->uy();

    gui::Painter::Color color = gui::Painter::dark_green;
    color.a = 180;
    painter.setBrush(color);
    painter.drawRect({lx, ly, ux, uy});
  }
}

void Graphics::drawLinden(gui::Painter& painter)
{
  drawBounds(painter);
  // Draw the placeable objects
  painter.setPen(gui::Painter::white);
  for (auto& cell : linden_db_->cells()) {
    if (cell.movable() && cell.is_instance()) {
      odb::dbInst* inst = static_cast<odb::dbInst*>(cell.odb_instance());
      inst->setPlacementStatus(odb::dbPlacementStatus::PLACED);
      const linden::geom::Rect& r = cell.rect();
      // pad awareness on X coordinates
      inst->setLocation(r.lx() + pb_->siteSizeX() * pb_->padLeft(), r.ly());
    }
  }
  for (auto& cell : linden_db_->cells()) {
    gui::Painter::Color color;
    if (cell.odb_instance()) {
      color
          = cell.locked() ? gui::Painter::dark_cyan : gui::Painter::dark_green;
    } else if (cell.is_virtual() && !cell.fixed()) {  // filler
      color = gui::Painter::dark_magenta;
    }
#if 0 // TODO: support select on linden-cell
    if (cell == selected_) {
      color = gui::Painter::yellow;
    }
#endif

    color.a = 180;
    painter.setBrush(color);
    auto const& r = cell.rect();
    painter.drawRect({r.lx(), r.ly(), r.ux(), r.uy()});
  }
}

void Graphics::drawNesterov(gui::Painter& painter)
{
  drawBounds(painter);
  if (draw_bins_) {
    // Draw the bins
    painter.setPen(gui::Painter::white, /* cosmetic */ true);
    for (auto& bin : nb_->bins()) {
      int color = bin->density() * 50 + 20;

      color = (color > 255) ? 255 : (color < 20) ? 20 : color;
      color = 255 - color;

      painter.setBrush({color, color, color, 180});
      painter.drawRect({bin->lx(), bin->ly(), bin->ux(), bin->uy()});
    }
  }

  // Draw the placeable objects
  painter.setPen(gui::Painter::white);
  for (auto* gCell : nb_->gCells()) {
    const int gcx = gCell->dCx();
    const int gcy = gCell->dCy();

    int xl = gcx - gCell->dx() / 2;
    int yl = gcy - gCell->dy() / 2;
    int xh = gcx + gCell->dx() / 2;
    int yh = gcy + gCell->dy() / 2;

    gui::Painter::Color color;
    if (gCell->isInstance()) {
      color = gCell->instance()->isLocked() ? gui::Painter::dark_cyan
                                            : gui::Painter::dark_green;
    } else if (gCell->isFiller()) {
      color = gui::Painter::dark_magenta;
    }
    if (gCell == selected_) {
      color = gui::Painter::yellow;
    }

    color.a = 180;
    painter.setBrush(color);
    painter.drawRect({xl, yl, xh, yh});
  }

  // Draw lines to neighbors
  if (selected_) {
    painter.setPen(gui::Painter::yellow, true);
    for (GPin* pin : selected_->gPins()) {
      GNet* net = pin->gNet();
      if (!net) {
        continue;
      }
      for (GPin* other_pin : net->gPins()) {
        GCell* neighbor = other_pin->gCell();
        if (neighbor == selected_) {
          continue;
        }
        painter.drawLine(
            pin->cx(), pin->cy(), other_pin->cx(), other_pin->cy());
      }
    }
  }

  // Draw force direction lines
  if (draw_bins_) {
    float efMax = 0;
    int max_len = std::numeric_limits<int>::max();
    for (auto& bin : nb_->bins()) {
      efMax
          = std::max(efMax, hypot(bin->electroForceX(), bin->electroForceY()));
      max_len = std::min({max_len, bin->dx(), bin->dy()});
    }

    for (auto& bin : nb_->bins()) {
      float fx = bin->electroForceX();
      float fy = bin->electroForceY();
      float f = hypot(fx, fy);
      float ratio = f / efMax;
      float dx = fx / f * max_len * ratio;
      float dy = fy / f * max_len * ratio;

      int cx = bin->cx();
      int cy = bin->cy();

      painter.setPen(gui::Painter::red, true);
      painter.drawLine(cx, cy, cx + dx, cy + dy);
    }
  }
}

void Graphics::drawObjects(gui::Painter& painter)
{
  if (nb_) {
    drawNesterov(painter);
  } else if (linden_db_) {
    drawLinden(painter);
  } else {
    drawInitial(painter);
  }
}

void Graphics::reportSelected()
{
  if (!selected_) {
    return;
  }
  auto instance = selected_->instance();
  if (instance && instance->dbInst()) {
    logger_->report("Inst: {}", instance->dbInst()->getName());
  }

  if (np_) {
    auto wlCoeffX = np_->getWireLengthCoefX();
    auto wlCoeffY = np_->getWireLengthCoefY();

    logger_->report("  Wire Length Gradient");
    for (auto& gPin : selected_->gPins()) {
      FloatPoint wlGrad
          = nb_->getWireLengthGradientPinWA(gPin, wlCoeffX, wlCoeffY);
      logger_->report("          ({:+.2e}, {:+.2e}) pin {}",
                      wlGrad.x,
                      wlGrad.y,
                      gPin->pin()->name());
    }

    FloatPoint wlGrad
        = nb_->getWireLengthGradientWA(selected_, wlCoeffX, wlCoeffY);
    logger_->report("  sum wl  ({: .2e}, {: .2e})", wlGrad.x, wlGrad.y);

    auto densityGrad = nb_->getDensityGradient(selected_);
    float densityPenalty = np_->getDensityPenalty();
    logger_->report("  density ({: .2e}, {: .2e}) (penalty: {})",
                    densityPenalty * densityGrad.x,
                    densityPenalty * densityGrad.y,
                    densityPenalty);
    logger_->report("  overall ({: .2e}, {: .2e})",
                    wlGrad.x + densityPenalty * densityGrad.x,
                    wlGrad.y + densityPenalty * densityGrad.y);
  }
}

void Graphics::cellPlot(bool pause)
{
  gui::Gui::get()->redraw();
  if (pause) {
    reportSelected();
    gui::Gui::get()->pause();
  }
}

gui::SelectionSet Graphics::select(odb::dbTechLayer* layer,
                                   const odb::Rect& region)
{
  selected_ = nullptr;

  if (layer || !nb_) {
    return gui::SelectionSet();
  }

  for (GCell* cell : nb_->gCells()) {
    const int gcx = cell->dCx();
    const int gcy = cell->dCy();

    int xl = gcx - cell->dx() / 2;
    int yl = gcy - cell->dy() / 2;
    int xh = gcx + cell->dx() / 2;
    int yh = gcy + cell->dy() / 2;

    if (region.xMax() < xl || region.yMax() < yl || region.xMin() > xh
        || region.yMin() > yh) {
      continue;
    }

    selected_ = cell;
    gui::Gui::get()->redraw();
    if (cell->isInstance()) {
      reportSelected();
      return {gui::Gui::get()->makeSelected(cell->instance()->dbInst())};
    }
  }
  return gui::SelectionSet();
}

void Graphics::status(const std::string& message)
{
  gui::Gui::get()->status(message);
}

/* static */
bool Graphics::guiActive()
{
  return gui::Gui::enabled();
}

}  // namespace gpl
