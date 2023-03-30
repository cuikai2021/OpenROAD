#include "linden/scanchain.hpp"
#include "sta/Liberty.hh"
#include "ord/OpenRoad.hh"
#include "db_sta/dbSta.hh"
#include "db_sta/dbNetwork.hh"
#include "utl/Logger.h"
#include "fassert.hpp"

using namespace std;

namespace linden {

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
        res.emplace_back(db_inst, lib_cell);
    }

    // sort by name to make them stable.
    // Ordering makes little sense.
    sort(
        res.begin(),
        res.end(),
        [](const PairedCell& a, const PairedCell& b) noexcept {
            if (a.name().size() < b.name().size()) {
                return true;
            } else if (a.name().size() > b.name().size()) {
                return false;
            }
            return a.name() < b.name();
        });
    return res;
}

} // namespace linden
