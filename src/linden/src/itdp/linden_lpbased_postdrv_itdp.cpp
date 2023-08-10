#include "linden/linden_lpbased_postdrv_itdp.hpp"
#include "linden_incr_tdp.hpp"
#include "load_timing_data.hpp"
#include "linden/db/db.hpp"
#include "sta/StaMain.hh"
#include "odb/db.h"
#include "ord/OpenRoad.hh"
#include "incr_tdp_data.pb.h"
#include "fassert.hpp"
#include <iostream>
#include <fstream>
#include <unordered_map>

using namespace std;
using namespace odb;

namespace {

struct LocalIncrTdpData {
    deque<linden::tdp::TimingArc> arcs;
    deque<linden::tdp::Port> ports;
    deque<linden::tdp::SeqPin> seq_pins;
    double wire_cap;
    double wire_res;

    LocalIncrTdpData()
    :   wire_cap(0),
        wire_res(0)
    {}

    void validate() const noexcept;
};

void LocalIncrTdpData::validate() const noexcept {
    for (auto const& pin: seq_pins) {
        FASSERT(pin.is_start ^ pin.is_end);
    }
    for (auto const& port: ports) {
        FASSERT(port.is_input ^ port.is_output);
    }
}

LocalIncrTdpData read_pb(linden::db::Database& ldb, const char* path) {
    string pb_file(path);
    cout << "pb file: " << pb_file << endl;
    ifstream fin(pb_file);
    lrl_tdp::IncrTdpData incr_tdp_data;
    bool ret = incr_tdp_data.ParseFromIstream(&fin);
    FASSERT(ret);

    std::unordered_map<std::string, const linden::db::Cell*> name2cell;
    for (auto& cell: ldb.cells()){
        name2cell[cell.name()] = &cell;
    }

    auto name2pin = [&](const linden::db::Cell& cell, const std::string& pin_name) -> linden::db::Pin* {
        for (auto& pin: cell.pins()) {
            if (pin.get().name() == pin_name) {
                return &pin.get();
            }
        }
        return nullptr;
    };
    auto pinname2net = [&](const linden::db::Cell& cell, const std::string& pin_name) -> odb::dbNet* {
        odb::dbInst* inst = static_cast<odb::dbInst*>(cell.odb_instance());
        for (auto iterm : inst->getITerms()) {
            if (iterm->getMTerm()->getName() == pin_name) {
                return iterm->getNet();
            }
        }
        return nullptr;
    };
    
    auto validate_arc_pins = [&](const linden::tdp::TimingArc& res_arc,
        const linden::db::Cell& cell,
        const lrl_tdp::Arc& pb_arc
    ) {
        if (res_arc.input_pin == nullptr) {
            cout << "WARNING: " << pb_arc.instance() + "/" + pb_arc.pin_start();
            cout << " cannot be found in Linden DB." << endl;
            auto odb_net = pinname2net(cell, pb_arc.pin_start());
            FASSERT(odb_net);
            auto net_type = odb_net->getSigType();
            FASSERT(net_type == odb::dbSigType::GROUND || net_type == odb::dbSigType::POWER);
            cout << "It is connected to VDD/VSS." << endl;
            return false;
        }
        if (res_arc.output_pin == nullptr) {
            cout << "WARNING: " << pb_arc.instance() + "/" + pb_arc.pin_end();
            cout << " cannot be found in Linden DB." << endl;
            auto odb_net = pinname2net(cell, pb_arc.pin_end());
            FASSERT(!odb_net
                    || (odb_net->getSigType() != odb::dbSigType::SIGNAL
                        && odb_net->getSigType() != odb::dbSigType::CLOCK))
            (odb_net->getName());
            cout << "The output pin does not belong to any net." << endl;
            return false;
        }
        return true;
    };

    auto validate_seq_pin = [&](const linden::tdp::SeqPin& res_seq_pin,
        const linden::db::Cell& cell,
        const lrl_tdp::SeqPin& pb_seq_pin
    ) {
        if (res_seq_pin.pin == nullptr) {
            cout << "WARNING: " << pb_seq_pin.instance() + "/" + pb_seq_pin.pin();
            cout << " cannot be found in Linden DB." << endl;
            auto odb_net = pinname2net(cell, pb_seq_pin.pin());
            FASSERT(odb_net);
            auto net_type = odb_net->getSigType();
            FASSERT(net_type == odb::dbSigType::GROUND || net_type == odb::dbSigType::POWER);
            cout << "It is connected to VDD/VSS." << endl;
            return false;
        }
        return true;
    };
    
    LocalIncrTdpData res;
    for(int i = 0, sz = incr_tdp_data.arcs_size(); i < sz; ++i) {
        auto const& pb_arc = incr_tdp_data.arcs(i);
        linden::tdp::TimingArc res_arc;
        auto pos = name2cell.find(pb_arc.instance());
        FASSERT(pos != name2cell.end())(pb_arc.instance());
        auto found_cell = pos->second;
        res_arc.input_pin = name2pin(*found_cell, pb_arc.pin_start());
        res_arc.output_pin = name2pin(*found_cell, pb_arc.pin_end());
        if (!validate_arc_pins(res_arc, *found_cell, pb_arc)) {
            continue;
        }

        res_arc.input_slack = pb_arc.input_slack();
        res_arc.output_slack = pb_arc.output_slack();
        res_arc.input_arr = pb_arc.input_arr();
        res_arc.output_arr = pb_arc.output_arr();
        res_arc.input_slew = pb_arc.input_slew();
        res_arc.output_slew = pb_arc.output_slew();
        res_arc.arc_delay = pb_arc.arc_delay();
        res_arc.cmax = pb_arc.c_max();
        res_arc.cpins = pb_arc.c_pins();
        res_arc.delay_args[0] = pb_arc.delay_args(0);
        res_arc.delay_args[1] = pb_arc.delay_args(1);
        res_arc.transition_args[0] = pb_arc.transition_args(0);
        res_arc.transition_args[1] = pb_arc.transition_args(1);

        res.arcs.emplace_back(res_arc);
    }

    auto is_clk_port = [](const lrl_tdp::Port& pb_port) {
        // ignore clock related ports such as "HCLK"
        // FIXME: port_type should be in pb file
        if (pb_port.port_name().find("CLK") != std::string::npos ||
            pb_port.port_name().find("clk") != std::string::npos)
        {
            cout << "Ignored port: " << pb_port.port_name() << endl;
            return true;
        }
        return false;
    };

    std::unordered_map<std::string, const linden::db::BTerm*> name2bterm;
    for (auto& pin: ldb.bterm_pins()) {
        name2bterm[pin.name()] = &pin;
    }
    
    for (int i = 0, sz = incr_tdp_data.ports_size(); i < sz; ++i) {
        auto const& pb_port = incr_tdp_data.ports(i);
        linden::tdp::Port res_port;
        res_port.bterm = nullptr;
        if (is_clk_port(pb_port)) {
            continue;
        }
        auto pos = name2bterm.find(pb_port.port_name());
        FASSERT(pos != name2bterm.end());
        res_port.bterm = pos->second;
        FASSERT(res_port.bterm != nullptr)(pb_port.port_name());
        res_port.is_input = pb_port.is_input();
        res_port.is_output = pb_port.is_output();
        res_port.cpins = pb_port.c_pins();
        res_port.cmax = pb_port.c_max();
        res_port.output_slack = pb_port.output_slack();
        res_port.arr = pb_port.arr();
        res_port.slew = pb_port.slew();

        res.ports.emplace_back(res_port);
    }

    for (int i = 0, sz = incr_tdp_data.seq_pins_size(); i < sz; ++i) {
        auto const& pb_seq_pin = incr_tdp_data.seq_pins(i);
        linden::tdp::SeqPin res_seq_pin;
        res_seq_pin.pin = nullptr;
        auto pos = name2cell.find(pb_seq_pin.instance());
        FASSERT(pos != name2cell.end())(pb_seq_pin.instance());
        const linden::db::Cell* found_cell = pos->second;
        res_seq_pin.pin = name2pin(*found_cell, pb_seq_pin.pin());
        if (!validate_seq_pin(res_seq_pin, *found_cell, pb_seq_pin)) {
            continue;
        }

        res_seq_pin.is_start = pb_seq_pin.is_start();
        res_seq_pin.is_end = pb_seq_pin.is_end();
        res_seq_pin.slack = pb_seq_pin.slack();
        res_seq_pin.setup_arg = pb_seq_pin.setup_args();
        res_seq_pin.arr = pb_seq_pin.arr();
        res_seq_pin.slew = pb_seq_pin.slew();

        res.seq_pins.emplace_back(res_seq_pin);
    }

    res.wire_cap = incr_tdp_data.wire_cap();
    res.wire_res = incr_tdp_data.wire_res();
    
    res.validate();
    
    return res;
}

} // namespace

namespace itdp {

LPBasedPostDrvItdp::LPBasedPostDrvItdp()
:    db_(nullptr)
{
    db_ = ord::OpenRoad::openRoad()->getDb();
}

// FIXME: replace cout to be spdlog logging

void LPBasedPostDrvItdp::run(const char* pb_path) {
    shared_ptr<linden::db::Database> ldb = create_linden_db();
    LocalIncrTdpData local_incr_tdp_data = read_pb(*ldb, pb_path);

    cout << "TDP: Finished reading data from pb file" << endl;

    linden::tdp::TdpConfig config(local_incr_tdp_data.wire_cap,
                                  local_incr_tdp_data.wire_res,
                                  ldb->core_region());

    linden::tdp::timing_driven_cell_move(ldb,
                                        local_incr_tdp_data.arcs,
                                        local_incr_tdp_data.ports,
                                        local_incr_tdp_data.seq_pins,
                                        config);

    // update db
    for (auto& cell : ldb->cells()) {
        if (cell.movable() && cell.is_instance()) {
            odb::dbInst* inst = static_cast<odb::dbInst*>(cell.odb_instance());
            inst->setPlacementStatus(odb::dbPlacementStatus::PLACED);
            const linden::geom::Rect& r = cell.rect();
            // pad awareness on X coordinates
            inst->setLocation(r.lx(), r.ly());
        }
    }
}

} // namespace itdp
