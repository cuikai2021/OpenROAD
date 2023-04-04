#include "linden/scanchain.hpp"
#include "ord/OpenRoad.hh"
#include "db_sta/dbSta.hh"
#include "db_sta/dbNetwork.hh"
#include "sta/Liberty.hh"
#include "sta/FuncExpr.hh"
#include "sta/Sequential.hh"
#include "utl/Logger.h"
#include "fassert.hpp"
#include <iostream>

using namespace std;

namespace linden {

namespace {

// Sort by name to make them stable.
// Ordering makes little sense.
template<typename Coll>
void make_stable(Coll& cells) {
    sort(
        cells.begin(),
        cells.end(),
        [](auto const& a, auto const& b) noexcept {
            if (a.name().size() < b.name().size()) {
                return true;
            } else if (a.name().size() > b.name().size()) {
                return false;
            }
            return a.name() < b.name();
        });
}

void extract_pins_from_sta_expr(
    unordered_set<sta::LibertyPort*>& out,
    odb::dbInst& db,
    sta::FuncExpr& expr)
{
    switch(expr.op()) {
    case sta::FuncExpr::op_port: {
        sta::LibertyPort* lib_pin = expr.port();
        FASSERT(lib_pin != nullptr);
        out.insert(lib_pin);
        break;
    }
    case sta::FuncExpr::op_one:
    case sta::FuncExpr::op_zero: break;
    // suppose the expr does not go too deep.
    case sta::FuncExpr::op_not: {
        FASSERT(expr.left() != nullptr);
        extract_pins_from_sta_expr(out, db, *expr.left());
        break;
    }
    case sta::FuncExpr::op_and:
    case sta::FuncExpr::op_or:
    case sta::FuncExpr::op_xor: {
        FASSERT(expr.left() != nullptr);
        FASSERT(expr.right() != nullptr);
        extract_pins_from_sta_expr(out, db, *expr.left());
        extract_pins_from_sta_expr(out, db, *expr.right());
        break;
    }
    }
}
} // namespace

PairedPin::PairedPin(odb::dbITerm* db_pin, sta::LibertyPort* sta_pin)
:   db_pin_(*db_pin),
    sta_pin_(*sta_pin),
    name_(sta_pin->name())
{
    FASSERT(db_pin != nullptr);
    FASSERT(sta_pin != nullptr);
}

PairedCell::PairedCell(odb::dbInst* odb_inst, sta::LibertyCell* sta_cell)
:   odb_inst_(*odb_inst),
    sta_cell_(*sta_cell)
{
    FASSERT(odb_inst != nullptr);
    FASSERT(sta_cell != nullptr);

    name_ = odb_inst_.get().getName();

    init_scan_enable();
    init_clocks();
    init_resets();
}

void PairedCell::init_scan_enable() {
    sta::TestCell* test_cell = sta().testCell();
    if(test_cell == nullptr) {
        return;
    }
    sta::LibertyPort* lib_port = test_cell->scanEnable();
    odb::dbITerm* db_pin = db().findITerm(lib_port->name());
    PairedPin se(db_pin, lib_port);
    scan_enable_ = std::move(se);
}

void PairedCell::init_clocks() {
    // Although LEF defines CLOCK in signal type, but it is usually unused.
    // Clocks are crucial to timing and power analysis.
    // So we consider liberty's answer as golden.
    for(auto db_pin: db().getITerms()) {
        auto lib_pin = sta().findLibertyPort(db_pin->getMTerm()->getConstName());
        if (lib_pin == nullptr) {
            continue;
        }
        if (!lib_pin->isClock()) {
            continue;
        }
        clocks_.emplace_back(db_pin, lib_pin);
    }
}

void PairedCell::init_resets() {
    if (!sta().hasSequentials()) {
        return;
    }

    unordered_set<sta::LibertyPort*> resets;
    sta::LibertyCellPortIterator iter(&sta());
    for(; iter.hasNext(); ) {
        sta::LibertyPort* port = iter.next();
        FASSERT(port != nullptr)
            (name());
        sta::Sequential* seq = sta().outputPortSequential(port);
        if (seq == nullptr) {
            continue;
        }
        sta::FuncExpr* clear = seq->clear();
        if (clear != nullptr) {
            extract_pins_from_sta_expr(resets, db(), *clear);
        }
        sta::FuncExpr* preset = seq->preset();
        if (preset != nullptr) {
            extract_pins_from_sta_expr(resets, db(), *preset);
        }
    }

    for(auto lib_pin: resets) {
        odb::dbITerm* db_pin = db().findITerm(lib_pin->name());
        resets_.emplace_back(db_pin, lib_pin);
    }
    make_stable(resets_);
}

deque<PairedCell> cells_on_scanchains() {
    ord::OpenRoad* ord = ord::OpenRoad::openRoad();

    odb::dbDatabase& db = *ord->getDb();
    unordered_set<odb::dbInst*> cells_on_sc;
    for (auto sc : db.getChip()->getBlock()->getScanChains()) {
        cells_on_sc.insert(sc->getStart()->getInst());
        cells_on_sc.insert(sc->getStop()->getInst());
        for (auto comp : sc->getFloating()) {
            cells_on_sc.insert(comp->getScanIn()->getInst());
        }
        for (auto comp : sc->getOrdered()) {
            cells_on_sc.insert(comp->getComps().begin()->getScanIn()->getInst());
        }
    }

    deque<PairedCell> res;
    auto sta = ord->getSta();
    sta::dbNetwork& network = *sta->getDbNetwork();
    for (auto db_inst : cells_on_sc) {
        sta::LibertyCell* lib_cell = network.libertyCell(db_inst);
        if (lib_cell != nullptr) {
            res.emplace_back(db_inst, lib_cell);
        }
    }

    make_stable(res);

    return res;
}

deque<PairedCell> all_cells_with_lib()
{
    ord::OpenRoad* ord = ord::OpenRoad::openRoad();
    odb::dbDatabase& db = *ord->getDb();
    sta::dbSta& sta = *ord->getSta();
    sta::dbNetwork& network = *sta.getDbNetwork();

    deque<PairedCell> res;
    for(auto db_inst: db.getChip()->getBlock()->getInsts()) {
        sta::LibertyCell* lib_cell = network.libertyCell(db_inst);
        if (lib_cell == nullptr) {
            continue;
        }
        res.emplace_back(db_inst, lib_cell);
    }

    make_stable(res);

    return res;
}

} // namespace linden
