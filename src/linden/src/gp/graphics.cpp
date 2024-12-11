#include "graphics.hpp"
#include "../util/odb_util.hpp"
#include "linden/db/cell.hpp"
#include "gui/gui.h"
#include "envflag/envflag.hpp"
#include <spdlog/spdlog.h>

IMPORT_I64_FLAG_WITH_DEFAULT(LINDEN_GUI_REFRESH);
IMPORT_I64_FLAG_WITH_DEFAULT(LINDEN_GUI_PAUSE);

using namespace std;

namespace linden::gp {

namespace {

void draw_unfilled_rect(gui::Painter& painter, const linden::geom::Rect& rect) {
    const linden::geom::Point ll = rect.ll();
    const linden::geom::Point ur = rect.ur();
    const linden::geom::Point corners[] = {
        ll,
        linden::geom::Point(ll.x(), ur.y()),
        ur,
        linden::geom::Point(ur.x(), ll.y()),
    };
    for(size_t i = 0, n = sizeof(corners)/sizeof(corners[0]); i < n; ++i) {
        auto const& start = corners[i];
        auto const& end = corners[(i + 1) % n];
        painter.drawLine(start.x(), start.y(), end.x(), end.y());
    }
}

} // namespace

class GraphicsInner: public gui::Renderer {
public:
    explicit GraphicsInner(shared_ptr<linden::db::Database> db);
    ~GraphicsInner() =default;

    void drawObjects(gui::Painter&) override;

private:
    void draw_boundaries(gui::Painter&) const;
    void draw_insts(gui::Painter&) const;
    void draw_macros(gui::Painter&, deque<linden::db::ConstCellRef>) const;
    void draw_fillers(gui::Painter&, deque<linden::db::ConstCellRef>) const;
    void draw_fixed(gui::Painter&, deque<linden::db::ConstCellRef>) const;
    void draw_stdcells(gui::Painter&, deque<linden::db::ConstCellRef>) const;

private:
    shared_ptr<linden::db::Database> db_;
};

GraphicsInner::GraphicsInner(shared_ptr<linden::db::Database> db)
:   db_(db)
{}

void GraphicsInner::drawObjects(gui::Painter& painter) {
    draw_boundaries(painter);
    draw_insts(painter);
}

void GraphicsInner::draw_boundaries(gui::Painter& painter) const
{
    painter.saveState();
    auto const& core = db_->core_region();
    painter.setPen(gui::Painter::yellow, /* cosmetic */ true);
    draw_unfilled_rect(painter, core);
    painter.restoreState();
}

void GraphicsInner::draw_insts(gui::Painter& painter) const
{
    deque<linden::db::ConstCellRef> fixed;
    deque<linden::db::ConstCellRef> stdcells;
    deque<linden::db::ConstCellRef> macros;
    for(auto const& x: db_->moveable_insts()) {
        auto const& cell = x.get();
        if (cell.macro()) {
            macros.emplace_back(cell);
        } else {
            stdcells.emplace_back(cell);
        }
    }
    draw_fixed(painter, std::move(db_->fixed_insts()));
    draw_stdcells(painter, std::move(stdcells));
    draw_macros(painter, std::move(macros));
    draw_fillers(painter, std::move(db_->fillers()));
}

void GraphicsInner::draw_fixed(
    gui::Painter& painter,
    deque<linden::db::ConstCellRef> fixed) const
{
    painter.saveState();
    for(auto const& x: fixed) {
        auto const& cell = x.get();
        if (cell.density() < 1.0) {
            painter.setPen(gui::Painter::light_gray, true);
        } else {
            painter.setPen(gui::Painter::gray, true);
        }
        painter.drawRect(lrect_to_orect(cell.rect()));
    }
    painter.restoreState();
}

void GraphicsInner::draw_stdcells(
    gui::Painter& painter,
    deque<linden::db::ConstCellRef> insts) const
{
    painter.saveState();
    gui::Painter::Color color = gui::Painter::dark_green;
    color.a = 180;
    painter.setBrush(color);
    for(auto const& x: insts) {
        auto const& cell = x.get();
        painter.drawRect(lrect_to_orect(cell.rect()));
    }
    painter.restoreState();
}

void GraphicsInner::draw_macros(
    gui::Painter& painter,
    deque<linden::db::ConstCellRef> macros) const
{
    painter.saveState();
    gui::Painter::Color color = gui::Painter::dark_green;
    color.a = 90;
    painter.setPen(color);
    for(auto const& x: macros) {
        auto const& cell = x.get();
        painter.drawRect(lrect_to_orect(cell.rect()));
    }
    painter.restoreState();
}

void GraphicsInner::draw_fillers(
    gui::Painter& painter,
    deque<linden::db::ConstCellRef> fillers) const
{
    painter.saveState();
    gui::Painter::Color color = gui::Painter::dark_magenta;
    color.a = 180;
    painter.setBrush(color);
    painter.setPen(color);
    for(auto const& x: fillers) {
        auto const& cell = x.get();
        painter.drawRect(lrect_to_orect(cell.rect()));
    }
    painter.restoreState();
}

Graphics::Graphics(shared_ptr<linden::db::Database> db)
:   inner_(new GraphicsInner(std::move(db)))
{
    gui::Gui::get()->registerRenderer(inner_);
}

Graphics::~Graphics() {
    gui::Gui::get()->unregisterRenderer(inner_);
    delete inner_;
}

bool Graphics::enabled() noexcept {
    return gui::Gui::enabled();
}

void Graphics::one_more_step(size_t iter) noexcept {
    bool update = (iter % LINDEN_GUI_REFRESH() == 0);
    bool pause = (iter % LINDEN_GUI_PAUSE() == 0);
    if (!update) {
        return;
    }
    gui::Gui::get()->redraw();
    if (pause) {
        gui::Gui::get()->pause();
    } else {
        // There is bugs for recent ORD (until f1a408763 at least).
        // 1. `gui::Gui::get()->redraw()` does not really redraw anything.
        //    But `gui::Gui::get()->pause()` does.
        // 2. A call to `gui::Gui::get()->pause()` with a timeout will make
        //    following calls without timeout nonsense.
        // So, enable the following will refresh every updates but no pause anymore.

        // gui::Gui::get()->pause(300);
    }
}

void Graphics::finalize() noexcept {
    gui::Gui::get()->redraw();
    gui::Gui::get()->pause();
}

} // namespace linden::gp
