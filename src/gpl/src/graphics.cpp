#include "graphics.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <limits>
#include <cstdlib>
#include <cstring>
#include <charconv>

#include "fassert.hpp"
#include "linden/db/db.hpp"
#include "linden/rdp/congestion_estimation.hpp"
#include "nesterovBase.h"
#include "nesterovPlace.h"
#include "ord/OpenRoad.hh"
#include "placerBase.h"
#include "utl/Logger.h"

namespace gpl {

Graphics::Graphics(utl::Logger* logger, std::shared_ptr<PlacerBase> pb)
    : pb_(pb),
      nb_(),
      np_(nullptr),
      linden_db_(nullptr),
      banyan_cmap_result_(nullptr),
      selected_(nullptr),
      draw_bins_(false),
      logger_(logger),
      rand_engine_((std::random_device())())
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
      banyan_cmap_result_(nullptr),
      selected_(nullptr),
      draw_bins_(draw_bins),
      logger_(logger),
      rand_engine_((std::random_device())())
{
  gui::Gui::get()->registerRenderer(this);
}

Graphics::Graphics(
    utl::Logger* logger,
    std::shared_ptr<linden::db::Database> linden_db,
    std::shared_ptr<linden::rdp::CongestionMap> banyan_cmap_result,
    std::shared_ptr<PlacerBase> pb)
    : pb_(pb),
      nb_(),
      np_(nullptr),
      linden_db_(linden_db),
      banyan_cmap_result_(banyan_cmap_result),
      selected_(nullptr),
      draw_bins_(false),
      logger_(logger),
      rand_engine_((std::random_device())())
{
  gui::Gui::get()->registerRenderer(this);
}

Graphics::Graphics(utl::Logger* logger,
           std::shared_ptr<linden::db::Database> linden_db)
    : pb_(),
      nb_(),
      np_(nullptr),
      linden_db_(linden_db),
      banyan_cmap_result_(nullptr),
      selected_(nullptr),
      draw_bins_(false),
      logger_(logger),
      rand_engine_((std::random_device())())
{
  gui::Gui::get()->registerRenderer(this);
}

void drawUnfilledRect(gui::Painter& painter,
                      int64_t lx,
                      int64_t ly,
                      int64_t ux,
                      int64_t uy)
{
  painter.drawLine(lx, ly, ux, ly);
  painter.drawLine(ux, ly, ux, uy);
  painter.drawLine(ux, uy, lx, uy);
  painter.drawLine(lx, uy, lx, ly);
}

void Graphics::drawBounds(gui::Painter& painter)
{
  // draw core bounds
  {
    auto db = ord::OpenRoad::openRoad()->getDb();
    odb::dbBlock* block = db->getChip()->getBlock();
    odb::Rect rec;
    block->getCoreArea(rec);
    painter.setPen(gui::Painter::yellow, /* cosmetic */ true);
    drawUnfilledRect(
        painter, rec.xMin(), rec.yMin(), rec.xMax(), rec.yMax());
  }
  // draw Blockages bounds
  auto db = ord::OpenRoad::openRoad()->getDb();
  painter.setPen(gui::Painter::yellow, true);
  for (auto blockage : db->getChip()->getBlock()->getBlockages()) {
    auto bbox = blockage->getBBox();
    if (blockage->isSoft()) {
      painter.setPen(gui::Painter::red, true);
    } else if (blockage->getMaxDensity() != 0) {  // partial blockage
      painter.setPen(gui::Painter::green, true);
    }
    drawUnfilledRect(
        painter, bbox->xMin(), bbox->yMin(), bbox->xMax(), bbox->yMax());
  }
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
  for (auto& inst : pb_->dummyInsts()) {
    int lx = inst->lx();
    int ly = inst->ly();
    int ux = inst->ux();
    int uy = inst->uy();

    gui::Painter::Color color = gui::Painter::gray;
    color.a = 180;
    painter.setBrush(color);
    painter.drawRect({lx, ly, ux, uy});
  }
}

class CongestionColoring
{
 public:
  static const int COLOR_CNT = 8;
  explicit CongestionColoring(const linden::rdp::CongestionMap& cmap)
      : cmap_(cmap)
  {
    // sort overflows
    std::vector<double> overflows;
    for (int j = 0; j < cmap.hgrids(); ++j) {
      for (int i = 0; i < cmap.wgrids(); ++i) {
        for (auto dim :
             {linden::geom::dimension::X, linden::geom::dimension::Y}) {
          auto overflow = cmap.overflow(dim, i, j);
          if (overflow > 0) {
            overflows.push_back(overflow);
          }
        }
      }
    }
    std::sort(overflows.begin(), overflows.end());

    // FIXME: @bangxiang: remove debugging code

    // for (int i = 0; i < overflows.size(); ++i) {
    //   std::cout << "overflows[" << i << "]:" << overflows[i] << std::endl;
    // }

    // init color_overflows_
    // std::cout << "**** COLOR_OVERFLOWS=" << std::endl;
    for (int i = 0; i < COLOR_CNT; ++i) {
      const auto sz = overflows.size();
      const auto delta = (sz + (COLOR_CNT - 1)) / COLOR_CNT;  // floor value
      auto pos = delta * (i + 1);
      pos = std::min(pos, sz - 1);
      color_overflows_[i] = overflows[pos];
      // std::cout << "\t" << i << ":" << color_overflows_[i] << " locate at:"
      // << pos << std::endl;
    }
  }

  gui::Painter::Color overflowColor(double overflow) const noexcept
  {
    // selected from https://www.rapidtables.com/web/color/RGB_Color.html
    // (colors from the right-2nd colunms)
    int64_t const COLOR_TABLE[COLOR_CNT] = {
        0xFFCCE5,
        0xFF99CC,
        0xFF66B2,
        0xFF3399,
        0xFF007F,
        0xCC0066,
        0x99004C,
        0x660033,
    };
    const int64_t COLOR_UNSET = 0xFFFFFF;
    const int64_t COLOR_MASKS[3] = {0xFF0000, 0x00FF00, 0x0000FF};
    int64_t color_val = COLOR_UNSET;
    for (int pos = 0; pos < COLOR_CNT; ++pos) {
      color_val = COLOR_TABLE[pos];
      if (overflow < color_overflows_[pos]) {
        break;
      }
    }
    FASSERT(color_val != COLOR_UNSET);
    int r = (COLOR_MASKS[0] & color_val) >> 16;
    int g = (COLOR_MASKS[1] & color_val) >> 8;
    int b = (COLOR_MASKS[2] & color_val);
    // std::cout << "**** Overflow Color: Overflow=" << overflow << ", Color=("
    //           << r << ", " << g << ", " << b << ")" << std::endl;
    return gui::Painter::Color{r, g, b, 0xff};
  }

 private:
  const linden::rdp::CongestionMap& cmap_;
  double color_overflows_[COLOR_CNT];
};

void Graphics::drawBanyanCMap(gui::Painter& painter)
{
  if (!banyan_cmap_result_ || banyan_cmap_result_->empty()) {
    return;
  }
  auto& cmap = *banyan_cmap_result_;
  CongestionColoring coloring(cmap);

  painter.setPen(gui::Painter::dark_red);
  const double dx = cmap.getChipCoreRegion().dx() / cmap.wgrids();
  const double dy = cmap.getChipCoreRegion().dy() / cmap.hgrids();

  for (auto dim : {linden::geom::dimension::X, linden::geom::dimension::Y}) {
    auto label = dim == linden::geom::dimension::X
                     ? DisplayControlLabel::Label::CMAP_H
                     : DisplayControlLabel::Label::CMAP_V;

    for (int j = 0; j < cmap.hgrids(); ++j) {
      for (int i = 0; i < cmap.wgrids(); ++i) {
        auto const& r = cmap.rect(i, j);
        auto overflow = cmap.overflow(dim, i, j);
        if (overflow > 0) {
          auto color = coloring.overflowColor(overflow);
          addRectToLabelSet({r.lx(), r.ly(), r.ux(), r.uy()}, color, label);
        }
      }
    }
  }
}

void Graphics::highlightFocus(
    const std::vector<std::vector<std::string>>& focus_names)
{
  for (int i = 0; i < focus_names.size(); i++) {
    for (int j = 0; j < focus_names[i].size(); j++) {
      gui::Gui::get()->addInstToHighlightSet(focus_names[i][j].c_str(), i);
    }
  }
  return;
}

void Graphics::drawLinden(gui::Painter& painter)
{
  drawBounds(painter);
  // Draw the placeable objects
  painter.setPen(gui::Painter::white);
  drawBanyanCMap(painter);
  for (auto& cell : linden_db_->cells()) {
    if (cell.movable() && cell.is_instance()) {
      odb::dbInst* inst = static_cast<odb::dbInst*>(cell.odb_instance());
      inst->setPlacementStatus(odb::dbPlacementStatus::PLACED);
      const linden::geom::Rect& r = cell.rect();
      // pad awareness on X coordinates
      inst->setLocation(r.lx() + pb_->siteSizeX() * pb_->padLeft(), r.ly());
    }
  }
#ifdef DEBUG_RDP_LABEL_SELECTION  // debug for label selection
  int index = 0;
#endif
  for (auto& cell : linden_db_->cells()) {
    gui::Painter::Color color;
    if (cell.odb_instance()) {
      color
          = cell.locked() ? gui::Painter::dark_cyan : gui::Painter::dark_green;
    } else if (cell.is_virtual()) {
      if (cell.fixed()) {
        // dummy instance which is to fill unusable sites
        // which will be placed to fill Macro halos, Partial|Soft blockages and
        // etc
        if (cell.max_density() == 0.0) {
          // dummy instances in Soft blockage
          color = gui::Painter::dark_gray;
        } else if (cell.max_density() < 1.0) {
          // dummy instances in Partial Blockage
          color = gui::Painter::light_gray;
        } else {
          // other cases, like Macro halos and etc
          FASSERT(cell.max_density() == 1.0);
          color = gui::Painter::gray;
        }
      } else {
        // movable filler added in linden
        color = gui::Painter::dark_magenta;
      }
    }
#if 0  // TODO: support select on linden-cell
    if (cell == selected_) {
      color = gui::Painter::yellow;
    }
#endif

    color.a = 180;
    painter.setBrush(color);
    auto const& r = cell.rect();
#ifdef DEBUG_RDP_LABEL_SELECTION  // debug for label selection
    std::vector<std::pair<gui::Painter::Color, DisplayControlLabel::Label>>
        labels{{gui::Painter::red, DisplayControlLabel::Label::LINDEN},
               {gui::Painter::blue, DisplayControlLabel::Label::CMAP_H},
               {gui::Painter::magenta, DisplayControlLabel::Label::CMAP_V},
               {gui::Painter::green, DisplayControlLabel::Label::EXPANDER},
               {gui::Painter::cyan, DisplayControlLabel::Label::MACRO}};
    index %= labels.size();
    addRectToLabelSet({r.lx(), r.ly(), r.ux(), r.uy()},
                      labels[index].first,
                      labels[index].second);
    index++;
#else
    addRectToLabelSet({r.lx(), r.ly(), r.ux(), r.uy()}, color, DisplayControlLabel::Label::LINDEN);
#endif
  }
}

namespace {
int getIntEnvValue(const char* envPath, int defaultValue)
{
  char* value = getenv(envPath);
  if(value == nullptr){
    return defaultValue;
  }

  utl::Logger* log_ = ord::OpenRoad::openRoad()->getLogger();
  int envValue;
  auto [ptr, ec] = std::from_chars(value, value + strlen(value), envValue);
  if(ec == std::errc() && ptr == value + strlen(value)){
    log_->report("Use env {}: {}", envPath, envValue);
    return envValue;
  } else {
    log_->report("Invalid environment variable of {}", envPath);
    abort();
  }
}

void symmetry(odb::Point& p1, odb::Point& p2, const linden::geom::Rect& r)
{
  int disy = p1.getY() - r.ly();
  int disx = p1.getX() - r.lx();
  p1.setX(r.lx() + disy);
  p1.setY(r.ly() + disx);

  disy = p2.getY() - r.ly();
  disx = p2.getX() - r.lx();
  p2.setX(r.lx() + disy);
  p2.setY(r.ly() + disx);
}

void flip(odb::Point& p1, odb::Point& p2, const linden::geom::Rect& r, linden::geom::dimension::Dimension dim)
{
  if(dim == linden::geom::dimension::X){
    p1.setY(r.ly() + r.uy() - p1.getY());
    p2.setY(r.ly() + r.uy() - p2.getY());
  } else {
    p1.setX(r.lx() + r.ux() - p1.getX());
    p2.setX(r.lx() + r.ux() - p2.getX());
  }
}

void drawMarcoOri(const linden::db::Cell& cell, gui::Painter& painter)
{
  painter.setPen(gui::Painter::white);
  painter.setPenWidth(1000);
  auto const& r = cell.rect();
  auto ori = cell.orientation();
  odb::Point p1(r.lx(), r.ly() + std::min(r.dx(), r.dy()) / 2);
  odb::Point p2(r.lx() + std::min(r.dx(), r.dy()) / 4, r.ly());

  switch(ori){
    case linden::db::W: {
      symmetry(p1, p2, r);
      flip(p1, p2, r, linden::geom::dimension::Y);
      break;
    }
    case linden::db::S: {
      flip(p1, p2, r, linden::geom::dimension::X);
      flip(p1, p2, r, linden::geom::dimension::Y);
      break;
    }
    case linden::db::E: {
      symmetry(p1, p2, r);
      flip(p1, p2, r, linden::geom::dimension::X);
      break;
    }
    case linden::db::FN: {
      flip(p1, p2, r, linden::geom::dimension::Y);
      break;
    }
    case linden::db::FE: {
      symmetry(p1, p2, r);
      break;
    }
    case linden::db::FS: {
      flip(p1, p2, r, linden::geom::dimension::X);
      break;
    }
    case linden::db::FW: {
      symmetry(p1, p2, r);
      flip(p1, p2, r, linden::geom::dimension::X);
      flip(p1, p2, r, linden::geom::dimension::Y);
      break;
    }
  }
  painter.drawLine(p1, p2);
}
} // namespace

void Graphics::drawLindenMP(gui::Painter& painter)
{
  drawBounds(painter);
  // Draw the placeable objects
  painter.setPen(gui::Painter::white);

  static int percent = getIntEnvValue("FILLER_PERCENT",10);
  static int transparency = getIntEnvValue("GRAPH_TRANSPARENCY", 180);
  for (auto& cell : linden_db_->cells()) {
    gui::Painter::Color color;
    if (cell.odb_instance()) {
      color
          = cell.locked() ? gui::Painter::dark_cyan : gui::Painter::dark_green; 
    } else if (cell.is_virtual() && !cell.fixed()) {  // filler
      std::uniform_int_distribution<int> uniform_num(1, 100);
      int random = uniform_num(rand_engine_);
      if(percent < 0 || percent > 100){
        logger_->report("Env FILLER_PERCENT out of range [0,100]");
        abort();
      }
      if(random > percent) {
        continue;
      }
      color = gui::Painter::dark_magenta;
    }
    
    color.a = transparency;
    painter.setBrush(color);
    auto const& r = cell.rect();
    painter.setPenWidth(1);
    painter.drawRect({r.lx(), r.ly(), r.ux(), r.uy()});
    if(cell.macro()){
      drawMarcoOri(cell, painter);
    }
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
    if(!pb_){
      drawLindenMP(painter);
    }else{
      drawLinden(painter);
    }
  } else {
    drawInitial(painter);
  }
  drawRectsInLabels(painter);
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

const char* Graphics::getDisplayControlGroupName()
{
  return "Labels";
}

std::vector<std::string> DisplayControlLabel::label_names
    = {"Linden", "Cmap-H", "Cmap-V", "Expander", "Macro"};

std::string DisplayControlLabel::label2name(const DisplayControlLabel::Label& l)
{
  FASSERT(l >= 0 && l < label_names.size());
  return label_names[l];
}

DisplayControlLabel::Label DisplayControlLabel::name2label(
    const std::string& name)
{
  auto pos = std::find(label_names.begin(), label_names.end(), name);
  assert(pos != label_names.end());
  return Label(pos-label_names.begin());
}

std::vector<DisplayControlLabel::Label> DisplayControlLabel::getOrderedLabels(
    const std::vector<std::string>& orderedLabelNames)
{
  std::vector<DisplayControlLabel::Label> res;
  for (const auto& name : orderedLabelNames) {
    res.emplace_back(name2label(name));
  }
  return res;
}

bool Graphics::isLablelSelected(const DisplayControlLabel::Label& l)
{
  return checkDisplayControl( DisplayControlLabel::label2name(l));
}

void Graphics::addRectToLabelSet(const odb::Rect& rect,
                                 const gui::Painter::Color& color,
                                 const DisplayControlLabel::Label& label)
{
  labeledRects_.push_back({rect, color, label});
}

void Graphics::drawRectsInLabels(gui::Painter& painter)
{
  auto orderedLabels = DisplayControlLabel::getOrderedLabels(
      gui::Gui::get()->getDisplayControlsOrderedLabelNames());
  // display bottom layers first
  for (int i = orderedLabels.size() - 1; i >= 0; i--) {
    DisplayControlLabel::Label label = orderedLabels[i];
    if (!isLablelSelected(label)) continue;
    for (const auto& obj : labeledRects_) {
      if (obj.label == label) {
        // draw obj
        painter.setBrush(obj.color);
        painter.drawRect(obj.rect);
      }
    }
  }
  labeledRects_.clear();
}

}  // namespace gpl
