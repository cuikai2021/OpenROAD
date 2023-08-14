#include "linden_incr_tdp.hpp"
#include "linden/tool/lpsolver.hpp"
#include "fassert.hpp"
#include "linden/geom/point.hpp"
#include "linden/geom/bbox.hpp"
#include <spdlog/spdlog.h>
#include <iostream>
#include <optional>
#include <cmath>
#include <numeric>
#include <utility>
#include <cfloat>

using namespace std;
using namespace linden::geom;
using namespace linden::db;
using namespace linden::tool::lpsolver;

namespace linden {
namespace tdp {

namespace {
// https://engineering.purdue.edu/~chengkok/ee695K/lec3a.pdf
const double K_D = 0.69;
const double K_S = 2.2;

const double length_unit = 1e-9; // unit conversion from nm to m (linden to Open STA)
const int64_t time_unit = 1e9;   // unit conversion from s to an arbitrary time unit
const int64_t s2ns_unit = 1e9;   // unit conversion from s to ns
const int64_t netlength_scaling_unit = 1e10;
const int64_t cload_scaling_unit = 1e3;
// REMARK: every length(wire-length, move distance, etc) is multiplied by length_unit
// then the right hand side of every constraint (i.e., the constant term) is multiplied by time_unit

// the default setting is 1e-7, the value should between [1e-7, WNS]
// , where the WNS is around 0.1 ~ 1ns
// FIXME: @Xudan to check the more accurate(smaller lp_error) after the stability issue is fixed.
const double lp_error = 1e-7;

// Decompose circuit into multiple subcircuits
// decomposing has benefit on: 1) runtime; 2) solvability
// TODO: This config var can be removed after we confirmed the correctness of decomposing.
// TODO: check the checker of decomposing.
const bool decompose = false;// fix it
    
}  // namespace

typedef std::pair<const Pin*, const Pin*> Segment;  // wire segment

struct Subcircuit {
    unordered_set<const Pin*> start_points;
    unordered_set<const Pin*> end_points;
    deque<TimingArc> timing_arcs;
    deque<Segment> segments;
    unordered_set<const Cell*> movable_cells;
    unordered_set<const Net*> movable_nets;
};

namespace {
string cell_variable(const Cell* cell, const string& var_name) {
    FASSERT(cell);
    // var_name here can be x|y
    return "cell" + cell->name() + var_name;
}

string net_variable(const Net* net, const string& var_name) {
    FASSERT(net);
    // var_name here can be lx|ly|ux|uy and delta_netlength|abs_delta_netlength
    return "net" + net->name() + var_name;
}

string get_pin_varname(const Pin* pin) {
    FASSERT(pin);
    auto is_bterm = pin->term_type() == Pin::TERM_TYPE::ODB_BTERM;
    string cell_name = is_bterm ? "bterm" : pin->cell()->get().name();
    return cell_name + '/' + pin->name();
}

string pin_variable(const Pin* pin, const string& var_name) {
    // var_name here can be delta_cload|inslope|outslope|arrival|required|slack|neg_slack
    return "pin" + get_pin_varname(pin) + var_name;
}

string arc_variable(const TimingArc& arc, const string& var_name) {
    // var_name here can be delta_delay
    auto inpin = arc.input_pin;
    auto outpin = arc.output_pin;
    FASSERT(inpin);
    FASSERT(outpin);
    auto cell = outpin->cell();
    FASSERT(cell.has_value());
    auto& cell_name = cell->get().name();
    return "arc" + cell_name + inpin->name() + outpin->name() + var_name;
}

string seg_variable(const Segment& seg, const string& var_name) {
    // var_name here can be lx|ly|ux|uy and delta_seglength|delta_delay|sq_delta_seglength
    return "seg" + get_pin_varname(seg.first) + get_pin_varname(seg.second) + var_name;
}

bool equal_pins(const Pin* p1, const Pin* p2) {
    return p1 == p2;
}

set<const Net*> cell2nets(const Cell* cell) {
    const auto& pins = cell->pins();
    FASSERT(!pins.empty());
    set<const Net*> nets;
    for (auto p : pins) {
        const auto& pin = p.get();
        const auto& net = pin.net();
        nets.insert(&net);
    }
    return nets;
}

int64_t hpwl(const Net* net) {
    vector<geom::Point> points;
    for (auto& pin : net->pins()) {
        points.emplace_back(pin.get().c(X), pin.get().c(Y));
    }
    auto const& result = geom::bbox(points.begin(), points.end());
    FASSERT(result);
    return result->half_perimeter();
}

int64_t find_seg_WL(const Segment& seg) {
    int64_t res = 0;
    for (auto dim : {X, Y}) {
        res += abs(seg.first->c(dim) - seg.second->c(dim));
    }
    return res;
}

// build the map from cell to its worst slack value
struct Cell2WorstSlackBuilder {
    Cell2WorstSlackBuilder(const deque<TimingArc>& timing_arcs, const deque<SeqPin>& seq_pins) {
        auto update_cell2ws = [&](const db::Pin* pin, double slack) {
            FASSERT(pin->cell().has_value());
            const Cell* cell = &(pin->cell()->get());
            if (!cell2ws.count(cell) || cell2ws.at(cell) > slack) {
                cell2ws[cell] = slack;
            }
        };

        for (auto& arc : timing_arcs) {
            update_cell2ws(arc.output_pin, arc.output_slack * s2ns_unit);
        }
        for (auto& seqpin : seq_pins) {
            update_cell2ws(seqpin.pin, seqpin.slack * s2ns_unit);
        }
    }

    // return sorted cells according to their slacks
    auto sorted_cells() const {
        deque<const Cell*> cells;
        for (auto& key_value : cell2ws) {
            cells.emplace_back(key_value.first);
        }

        // sort cell according to their slacks
        sort(cells.begin(), cells.end(), [&](const Cell* c1, const Cell* c2) {
            auto v1 = cell2ws.at(c1);
            auto v2 = cell2ws.at(c2);
            return v1 == v2 ? c1->name() < c2->name() : v1 < v2;
        });
        return cells;
    }

    unordered_map<const Cell*, double> cell2ws;
};

}  // namespace

// TODO
// void print_end_point_slack_distribution() {
// }

// print for cells
// in some cases, critical(or near critical) paths together contain more than 500 cells
// at least, we should know if this happens
void print_cell_slack_distribution(
    shared_ptr<Database> linden_db,
    const deque<TimingArc>& timing_arcs,
    const deque<SeqPin>& seq_pins)
{
    Cell2WorstSlackBuilder builder(timing_arcs, seq_pins);
    vector<double> slacks;
    for (auto [_, slack] : builder.cell2ws) {
        slacks.emplace_back(slack);
    }

    // sort slacks starting from the lowest
    sort(slacks.begin(), slacks.end());
    auto data_size = slacks.size();

    spdlog::info("Slack Data of cells (in ns): ");
    spdlog::info("Num of data points: {}", data_size);
    spdlog::info("Num of cells: {}", linden_db->cells().size());
    spdlog::info("WNS: {}", slacks[0]);
    if (data_size >= 500) {
        spdlog::info("Top 500 slack: [{}, {}]", slacks[0], slacks[499]);
    }
    for (auto p : {1, 5, 10, 20, 50}) {
        spdlog::info("Top {}% slack: {}", p, slacks[data_size / (100/p)]);
    }

    size_t cnt = count_if(slacks.begin(), slacks.end(), [](auto v) { return v < 0.0; });
    spdlog::info("Num of negative-slack cells: {}", cnt);
}

void report_delays(
    shared_ptr<Database> linden_db,
    const deque<TimingArc>& timing_arcs,
    const deque<Port>& ports,
    const deque<SeqPin>& seq_pins)
{   
    auto pin2arr = [&]() {
        unordered_map<const Pin*, double> pin2arr;
        auto update_pin2arr = [&](const Pin* pin, double arrival_time) {
            if (!pin2arr.count(pin)
                || pin2arr.at(pin) < arrival_time) {
                pin2arr[pin] = arrival_time;
            }
        };
        
        for (auto& arc : timing_arcs) {
            update_pin2arr(arc.input_pin, arc.input_arr);
            update_pin2arr(arc.output_pin, arc.output_arr);
        }
        for (auto& port : ports) {
            pin2arr.insert({port.bterm, port.arr});
        }
        for (auto& seqpin : seq_pins) {
            pin2arr.insert({seqpin.pin, seqpin.arr});
        }
        return pin2arr;
    }();

    // collect arc delays
    auto arc_delays= [&]() {
        deque<double> arc_delays;
        for (auto& arc : timing_arcs) {
            FASSERT(arc.arc_delay >= 0)(arc.arc_delay);
            arc_delays.emplace_back(arc.arc_delay);
        }
        return arc_delays;
    }();

    // collect wire delays
    auto wire_delays = [&]() {
        deque<double> wire_delays;
        auto collect_wire_delays = [&](const db::Pin* driver) {
            auto& net = driver->net();
            for (auto p : net.pins()) {
                auto pin = &p.get();
                if (equal_pins(driver, pin) || !pin2arr.contains(pin)) {
                    continue;
                }
                double wire_delay = pin2arr.at(pin) - pin2arr.at(driver);
                if (wire_delay < 0.0) {
                    spdlog::warn("Seqment: {} ---> {} has negative delay: {}",
                        get_pin_varname(driver), get_pin_varname(pin), wire_delay * time_unit);
                }
                wire_delays.emplace_back(wire_delay);
            }
        };

        for (auto& arc : timing_arcs) {
            collect_wire_delays(arc.output_pin);
        }

        for (auto& port : ports) {
            if (port.is_input) {
                collect_wire_delays(port.bterm);
            }
        }
        return wire_delays;
    }();

    auto print_delay_stats = [] (const std::string& desc, deque<double>& delays) {
        auto print_delay = [] (const std::string& prefix, double delay) {
            spdlog::info("{}: {}", prefix, delay * time_unit);
        };
        FASSERT(delays.size() > 0);

        sort(delays.begin(), delays.end(), greater<double>());
        const double mean = std::reduce(delays.begin(), delays.end()) / delays.size();
        spdlog::info("---------------------------------------------------------");
        spdlog::info("{} stats (in ns): num of data points: {}", desc, delays.size());
        print_delay("max: "    , *delays.begin());
        print_delay("min: "    , *delays.rbegin());
        print_delay("mean: "   , mean);
        for (auto p : {1, 5, 10, 20, 50}) {
            std::ostringstream prefix;
            prefix << "Top " << p << "%: ";
            print_delay(prefix.str(), delays[delays.size() / (100/p)]);
        }
    };
    print_delay_stats("arc delay", arc_delays);
    print_delay_stats("wire delay", wire_delays);
}

unordered_set<const Net*> get_clk_nets(const deque<SeqPin>& seq_pins)
{
    unordered_set<const Net*> clk_nets;
    for (auto& seqpin : seq_pins) {
        if (seqpin.is_start) {  // start point of reg is CK
            //FIXME： 直接根据名字来，太粗暴了。无论liberty里还是lef里，都会有pin type的信息，应该根据pin type来判断
            FASSERT(seqpin.pin->name() != "Q")("unexpected seq start pin from the pb file")(seqpin.pin->name());
            auto& net = seqpin.pin->net();
            clk_nets.insert(&net);
        }
    }
    return clk_nets;
}

// select the top 5% cells with the worst slack
// if 5% is greater than 500, then choose 500.
// also, do not take cells with positive slack into consideration
unordered_set<const Cell*>
select_movable_cells(
    shared_ptr<Database> linden_db,
    const deque<TimingArc>& timing_arcs,
    const deque<SeqPin>& seq_pins)
{
    const auto clk_nets = get_clk_nets(seq_pins);
    const auto non_clknet_driving_cells = [&]() {
        // cells that are driving clk_net
        unordered_set<const Cell*> non_clknet_driving_cells;
        for(auto arc:timing_arcs){
            auto driving_net = &arc.output_pin->net();
            if(!clk_nets.contains(driving_net)){
                non_clknet_driving_cells.insert(&arc.output_pin->cell()->get());
            }
        }
        return non_clknet_driving_cells;
    }();

    Cell2WorstSlackBuilder builder(timing_arcs, seq_pins);
    const auto cells = builder.sorted_cells();

    const int UPPER_BOUND = 200;
    size_t upper_bound = min(int(cells.size() * 0.05), UPPER_BOUND);
    unordered_set<const Cell*> movable_cells;
    for (auto cell : cells) {
        if (movable_cells.size() == upper_bound) {
            break;
        }
        if (builder.cell2ws[cell] >= 0.0) {
            break;
        }
        if (!cell->fixed() && non_clknet_driving_cells.contains(cell)) {
            movable_cells.insert(cell);
        }
    }
    return movable_cells;
}

// NOTE: This function is not reviewed seriously as it is not enabled yet.
[[maybe_unused]] void extend_movable_cells(
    shared_ptr<Database> linden_db,
    unordered_set<const Cell*>& movable_cells)
{
    // add extra layers of movable cells
    // TBD: detailed selection will be done later
    const size_t LAYER_NUMBER = 1;
    for (size_t i = 0; i < LAYER_NUMBER; ++i) {
        unordered_set<const Cell*> more_cells;
        for (auto cell : movable_cells) {
            auto nets = cell2nets(cell);
            for (auto net : nets) {
                auto pins = net->pins();
                for (auto p : pins) {
                    auto pin = &p.get();
                    if (!pin->cell().has_value()) {
                        continue;
                    }
                    auto new_cell = &(pin->cell()->get());
                    if (new_cell->movable()) {
                        more_cells.insert(new_cell);
                    }
                }
            }
        }

        for (auto cell : more_cells) {
            movable_cells.insert(cell);
        }
    }
}

unordered_set<const Net*> find_movable_nets(
    shared_ptr<Database> linden_db,
    const unordered_set<const Cell*>& movable_cells,
    const deque<SeqPin>& seq_pins)
{
    auto is_tie_net = [](auto net) {
        auto pins = net->pins();
        if (pins.size() != 2) {
            return false;
        }
        for (auto p : pins) {
            const auto& pcell = p.get().cell();
            if (pcell) {
                const auto& cell = pcell->get();
                if(cell.pins().size() == 1) {
                    FASSERT(cell.name().find("TIE") != std::string::npos)(cell.name());
                    return true;
                }
            }
        }
        return false;
    };

    const auto clk_nets = get_clk_nets(seq_pins);
    auto is_clk_net = [&](auto net) {
        return clk_nets.contains(net);
    };
    
    unordered_set<const Net*> movable_nets;
    for (auto cell : movable_cells) {
        auto const& nets = cell2nets(cell);
        std::copy_if(nets.begin(), nets.end(), std::inserter(movable_nets, movable_nets.begin()), [&](const auto& net) {
            // exclude tienet and clknet
            return !(is_tie_net(net) || is_clk_net(net));
        });
    }
    return movable_nets;
}

// only keep arcs and segs that could have changes
[[maybe_unused]] void shrink_problem_size(
    shared_ptr<Database> linden_db,
    const unordered_set<const Cell*>& movable_cells,
    const unordered_set<const Net*>& movable_nets,
    const deque<TimingArc>& timing_arcs,
    const deque<Port>& ports,
    const deque<SeqPin>& seq_pins,
    Subcircuit& output_subcircuit)
{
    auto& start_points = output_subcircuit.start_points;
    auto& end_points = output_subcircuit.end_points;
    auto& output_timing_arcs = output_subcircuit.timing_arcs;
    auto& output_segments = output_subcircuit.segments;
    output_subcircuit.movable_cells = movable_cells;
    output_subcircuit.movable_nets = movable_nets;

    // preparation: create pin2arc map
    unordered_map<const Pin*, deque<TimingArc*>> inpin2arc;
    unordered_map<const Pin*, deque<TimingArc*>> outpin2arc;
    for (auto& arc : timing_arcs) {
        TimingArc* arc_ptr = &const_cast<TimingArc&>(arc);
        auto inpin = arc.input_pin;
        if (!inpin2arc.contains(inpin)) {
            deque<TimingArc*> arcs;
            arcs.emplace_back(arc_ptr);
            inpin2arc.insert({inpin, arcs});
        } else {
            inpin2arc.at(inpin).emplace_back(arc_ptr);
        }

        auto outpin = arc.output_pin;
        if (!outpin2arc.contains(outpin)) {
            deque<TimingArc*> arcs;
            arcs.emplace_back(arc_ptr);
            outpin2arc.insert({outpin, arcs});
        } else {
            outpin2arc.at(outpin).emplace_back(arc_ptr);
        }
    }
    size_t size_tmp = 0;
    for (auto& pair : inpin2arc) {
        size_tmp += pair.second.size();
    }
    FASSERT(size_tmp == timing_arcs.size()) (size_tmp)(timing_arcs.size());

    // preparation: find initial drivers and receivers (of nets) as starting
    // points of BFS
    deque<const Pin*> drivers;
    deque<const Pin*> receivers;
    for (auto& arc : timing_arcs) {
        auto inpin = arc.input_pin;
        auto outpin = arc.output_pin;
        const Net* net = &(outpin->net());
        if (movable_nets.contains(net)) {
            // avoid repeated receivers (improve runtime later)
            if (find(receivers.begin(), receivers.end(), inpin) == receivers.end()) {
                receivers.emplace_back(inpin);
            }
        }
    }

    for (auto& port : ports) {
        if (port.is_input) {
            auto pin = port.bterm;
            const Net* net = &(pin->net());
            if (movable_nets.contains(net)) {
                drivers.emplace_back(pin);
            }
        }
    }

    // preparation: find endpoints
    // two types: (1)circuit ends and (2)previously reached pts
    // a search path in bfs should stop when it reaches any of these pts
    unordered_set<const Pin*> port_seqcell_ends;
    // record intermediate searched points
    unordered_set<const Pin*> bfs_intermediates;
    // record io pins, which are special: can be both start and end
    unordered_set<const Pin*> iopins;

    for (auto& port : ports) {
        if (port.is_output) {
            port_seqcell_ends.insert(port.bterm);
            if (port.is_input) {
                iopins.insert(port.bterm);
            }
        }
    }
    for (auto& seq_pin : seq_pins) {
        if (seq_pin.is_end) {
            port_seqcell_ends.insert(seq_pin.pin);
        }
    }
    for (auto pin : drivers) {
        start_points.insert(pin);
    }
    for (auto pin : receivers) {
        start_points.insert(pin);
    }

    // run bfs to get related segments and arcs
    while (!drivers.empty() || !receivers.empty()) {
        // get related segments from drivers
        while (!drivers.empty()) {
            auto seg_startpin = drivers.back();
            // skip if this point has been reached earlier
            if (bfs_intermediates.contains(seg_startpin)) {
                drivers.pop_back();
                continue;
            }
            auto& net = seg_startpin->net();
            for (auto const& p : net.pins()) {
                auto seg_endpin = &p.get();
                // skip p = pin
                if (equal_pins(seg_endpin, seg_startpin)) {
                    continue;
                }
                if (!iopins.contains(seg_endpin)) {
                    start_points.erase(seg_endpin);
                }
                // only add a receiver to deque if it is not an endpoint
                if (!port_seqcell_ends.contains(seg_endpin) &&
                    !bfs_intermediates.contains(seg_endpin))
                {
                    // avoid repeated receivers (improve runtime later)
                    if (find(receivers.begin(), receivers.end(), seg_endpin) ==
                        receivers.end())
                    {
                        receivers.emplace_back(seg_endpin);
                    }
                } else if (port_seqcell_ends.contains(seg_endpin)) {
                    end_points.insert(seg_endpin);
                }
                Segment seg = make_pair(seg_startpin, seg_endpin);
                output_segments.emplace_back(seg);
            }
            bfs_intermediates.insert(seg_startpin);
            drivers.pop_back();
        }

        // get related arcs from receivers
        while(!receivers.empty()) {
            auto arc_inpin = receivers.back();
            if (bfs_intermediates.contains(arc_inpin)) {
                receivers.pop_back();
                continue;
            }
            if (!inpin2arc.contains(arc_inpin)) {
                receivers.pop_back();
                bfs_intermediates.insert(arc_inpin);
                continue;
            }
            auto arcs = inpin2arc.at(arc_inpin);
            for (auto arc : arcs) {
                auto arc_outpin = arc->output_pin;
                output_timing_arcs.emplace_back(*arc);
                if (!iopins.contains(arc_outpin)) {
                    start_points.erase(arc_outpin);
                }
                // only add a driver to deque if it is not an endpoint
                FASSERT(!port_seqcell_ends.contains(arc_outpin));
                if (!bfs_intermediates.contains(arc_outpin))
                {
                    // avoid repeated drivers
                    if (find(drivers.begin(), drivers.end(), arc_outpin) ==
                        drivers.end())
                    {
                        drivers.emplace_back(arc_outpin);
                    }
                }
            }
            bfs_intermediates.insert(arc_inpin);
            receivers.pop_back();
        }
    }

    // need to add additional timing arcs (to correctly calculate arrival times)
    // for the case that several timing arcs share the same end point
    deque<const Pin*> arc_outpins;
    for (auto& arc : output_timing_arcs) {
        auto outpin = arc.output_pin;
        arc_outpins.emplace_back(outpin);
    }
    for (auto pin : arc_outpins) {
        auto arcs = outpin2arc.at(pin);
        // add arcs which are not added
        for (auto arc : arcs) {
            auto inpin = arc->input_pin;
            if (!bfs_intermediates.contains(inpin) &&
                !start_points.contains(inpin))
            {
                output_timing_arcs.emplace_back(*arc);
                start_points.insert(inpin);
            }
        }
    }
}

void construct_effective_subcirsuit(
    shared_ptr<Database> linden_db,
    const unordered_set<const Cell*>& movable_cells,
    const unordered_set<const Net*>& movable_nets,
    const deque<TimingArc>& timing_arcs,
    const deque<Port>& ports,
    const deque<SeqPin>& seq_pins,
    Subcircuit& output_subcircuit)
{
    auto& start_points = output_subcircuit.start_points;
    auto& end_points = output_subcircuit.end_points;
    auto& output_timing_arcs = output_subcircuit.timing_arcs;
    auto& output_segments = output_subcircuit.segments;
    output_subcircuit.movable_cells = movable_cells;
    output_subcircuit.movable_nets = movable_nets;

    unordered_map<const Pin*, deque<const TimingArc*>> pin2arcs;
    unordered_set<const Pin*> drivers;  // output pin of combinational cells
    for (auto& arc : timing_arcs) {
        auto inpin = arc.input_pin;
        auto outpin = arc.output_pin;
        drivers.insert(outpin);
        for (auto pin : {inpin, outpin}) {
            if (!pin2arcs.contains(pin)) {
                pin2arcs.insert({pin, {&arc}});
            } else {
                pin2arcs.at(pin).emplace_back(&arc);
            }
        }
    }

    unordered_map<const Pin*, const SeqPin*> pin2seq;
    unordered_map<const Pin*, const Port*> pin2port;
    for (auto& seqpin : seq_pins) {
        pin2seq.insert({seqpin.pin, &seqpin});
    }
    for (auto& port : ports) {
        pin2port.insert({port.bterm, &port});
    }

    // collect timing arcs, ports, and pins in nets
    unordered_set<const TimingArc*> add_arcs;
    for (auto net : movable_nets) {
        auto pins = net->pins();
        for (auto p : pins) {
            auto pin = &p.get();
            if (pin2arcs.contains(pin)) { // pin on timing arc
                auto& arcs = pin2arcs.at(pin);
                for (auto arc : arcs) {
                    add_arcs.insert(arc);
                }
            }
        }
    }

    // add arcs
    for (auto arc : add_arcs) {
        output_timing_arcs.emplace_back(*arc);
    }

    // collect net segments
    for (auto net : movable_nets) {
        auto pins = net->pins();
        const Pin* net_driver = nullptr;
        for (auto p : pins) {
            auto pin = &p.get();
            if (drivers.contains(pin)) {
                net_driver = pin;
                break;
            } else if (pin2port.contains(pin)) {
                if (pin2port.at(pin)->is_input) {
                    net_driver = pin;
                    break;
                }
            }
        }
        FASSERT(net_driver != nullptr) (net->name());
        for (auto p : pins) {
            auto pin = &p.get();
            if (equal_pins(pin, net_driver)) {
                continue;
            }
            Segment seg = make_pair(net_driver, pin);
            output_segments.emplace_back(seg);
        }
    }

    // find start points and end points
    for (auto& arc : output_timing_arcs) {
        auto inpin = arc.input_pin;
        auto netin = &(inpin->net());
        if (!movable_nets.contains(netin)) {
            start_points.insert(inpin);
        }
        auto outpin = arc.output_pin;
        auto netout = &(outpin->net());
        if (!movable_nets.contains(netout)) {
            // auto cell = &(outpin->cell()->get());
            // FASSERT(!movable_cells.contains(cell))
            //     (cell->name());
            end_points.insert(outpin);
        }
    }
    for (auto& port : ports) {
        auto pin = port.bterm;
        auto net = &(pin->net());
        if (!movable_nets.contains(net)) {
            continue;
        }
        if (port.is_input) {
            start_points.insert(pin);
        } else if (port.is_output) {
            end_points.insert(pin);
        }
    }
    for (auto& seqpin : seq_pins) {
        auto pin = seqpin.pin;
        auto net = &(pin->net());
        if (!movable_nets.contains(net)) {
            continue;
        }
        if (seqpin.is_end) {
            end_points.insert(pin);
        }
    }
}

deque<Subcircuit> decompose_problem(
    shared_ptr<Database> linden_db,
    const Subcircuit& large_subcircuit,
    const deque<Port>& ports,
    const deque<SeqPin>& seq_pins)
{
    auto& movable_cells = large_subcircuit.movable_cells;
    auto& movable_nets = large_subcircuit.movable_nets;
    auto& timing_arcs = large_subcircuit.timing_arcs;
    auto& segments = large_subcircuit.segments;
    auto& start_points = large_subcircuit.start_points;
    auto& end_points = large_subcircuit.end_points;

    deque<Subcircuit> subcircuits;

    // preparation: find all_related_cells
    unordered_set<const Cell*> all_related_cells;
    for (auto& arc : timing_arcs) {
        auto cell = &(arc.input_pin->cell()->get());
        all_related_cells.insert(cell);
    }

    // preparation: create pin2arc map
    unordered_map<const Pin*, deque<TimingArc*>> inpin2arc;
    unordered_map<const Pin*, deque<TimingArc*>> outpin2arc;
    for (auto& arc : timing_arcs) {
        TimingArc* arc_ptr = &const_cast<TimingArc&>(arc);
        auto inpin = arc.input_pin;
        if (!inpin2arc.contains(inpin)) {
            inpin2arc.insert({inpin, {arc_ptr}});
        } else {
            inpin2arc.at(inpin).emplace_back(arc_ptr);
        }

        auto outpin = arc.output_pin;
        if (!outpin2arc.contains(outpin)) {
            outpin2arc.insert({outpin, {arc_ptr}});
        } else {
            outpin2arc.at(outpin).emplace_back(arc_ptr);
        }
    }

    // preparation: create driver2segs map
    unordered_map<const Pin*, deque<Segment*>> driver2segs;
    for (auto& seg : segments) {
        Segment* seg_ptr = &const_cast<Segment&>(seg);
        auto driver = seg.first;
        if (!driver2segs.contains(driver)) {
            driver2segs.insert({driver, {seg_ptr}});
        } else {
            driver2segs.at(driver).emplace_back(seg_ptr);
        }
    }

    // perform bfs
    unordered_set<const Pin*> reached_start_points;
    for (auto start : start_points) {
        if (reached_start_points.contains(start)) {
            continue;
        }
        unordered_set<const Pin*> all_reached_pins;
        deque<const Pin*> pin_queue;
        pin_queue.emplace_back(start);
        while (!pin_queue.empty()) {
            auto vertex = pin_queue.front();
            all_reached_pins.insert(vertex);
            pin_queue.pop_front();
            // add its neighbors to queue
            auto net = &(vertex->net());
            if (movable_nets.contains(net)) {
                auto pins = net->pins();
                for (auto p : pins) {
                    auto pin = &p.get();
                    if (equal_pins(pin, vertex)) {
                        continue;
                    }
                    if (!all_reached_pins.contains(pin) &&
                        find(pin_queue.begin(), pin_queue.end(), pin) ==
                        pin_queue.end())
                    {
                        pin_queue.emplace_back(pin);
                    }
                }
            }
            auto cell_opt = vertex->cell();
            if (cell_opt.has_value()) {
                auto cell = &(cell_opt->get());
                // pins on non-movable cells
                if (!movable_cells.contains(cell)) {
                    if (inpin2arc.contains(vertex)) {
                        auto& arcs = inpin2arc.at(vertex);
                        for (auto arc : arcs) {
                            auto pin = arc->output_pin;
                            if (!all_reached_pins.contains(pin) &&
                                find(pin_queue.begin(), pin_queue.end(), pin) ==
                                pin_queue.end())
                            {
                                pin_queue.emplace_back(pin);
                            }
                        }
                    }
                    if (outpin2arc.contains(vertex)) {
                        auto& arcs = outpin2arc.at(vertex);
                        for (auto arc : arcs) {
                            auto pin = arc->input_pin;
                            if (!all_reached_pins.contains(pin) &&
                                find(pin_queue.begin(), pin_queue.end(), pin) ==
                                pin_queue.end())
                            {
                                pin_queue.emplace_back(pin);
                            }
                        }
                    }
                } else {    // pins on movable cells
                    auto pins = cell->pins();
                    for (auto p : pins) {
                        auto pin = &p.get();
                        if (equal_pins(pin, vertex)) {
                            continue;
                        }
                        if (!all_reached_pins.contains(pin) &&
                            find(pin_queue.begin(), pin_queue.end(), pin) ==
                            pin_queue.end())
                        {
                            pin_queue.emplace_back(pin);
                        }
                    }
                }
            }
        }
        for (auto pin : all_reached_pins) {
            if (start_points.contains(pin)) {
                reached_start_points.insert(pin);
            }
        }

        // create the corresponding subcircuit
        Subcircuit subcircuit;
        auto& sub_start_points = subcircuit.start_points;
        auto& sub_end_points = subcircuit.end_points;
        auto& sub_movable_cells = subcircuit.movable_cells;
        auto& sub_movable_nets = subcircuit.movable_nets;
        auto& sub_timing_arcs = subcircuit.timing_arcs;
        auto& sub_segments = subcircuit.segments;

        unordered_set<const TimingArc*> add_arcs;
        unordered_set<const Segment*> add_segs;

        for (auto pin : all_reached_pins) {
            if (start_points.contains(pin)) {
                sub_start_points.insert(pin);
            }
            if (end_points.contains(pin)) {
                sub_end_points.insert(pin);
            }
            auto cell_opt = pin->cell();
            if (cell_opt.has_value()) {
                auto cell = &(cell_opt->get());
                if (movable_cells.contains(cell)) {
                    sub_movable_cells.insert(cell);
                }
            }
            auto net = &(pin->net());
            if (movable_nets.contains(net)) {
                sub_movable_nets.insert(net);
            }
            if (inpin2arc.contains(pin)) {
                auto arcs = inpin2arc.at(pin);
                for (auto arc : arcs) {
                    add_arcs.insert(arc);
                }
            }
            if (driver2segs.contains(pin)) {
                auto segs = driver2segs.at(pin);
                for (auto seg : segs) {
                    add_segs.insert(seg);
                }
            }
        }

        for (auto arc : add_arcs) {
            sub_timing_arcs.emplace_back(*arc);
        }
        for (auto seg : add_segs) {
            sub_segments.emplace_back(*seg);
        }
        subcircuits.emplace_back(subcircuit);
    }
    return subcircuits;
}

void add_netlength_constraints(
    Problem<string>& prob,
    shared_ptr<Database> linden_db,
    const unordered_set<const Cell*>& movable_cells,
    const unordered_set<const Net*>& movable_nets,
    double cell_move_bound)
{
    auto unit = length_unit * time_unit;
    auto core = linden_db->core_region();
    for (auto cell : movable_cells) {
        auto cell_pos_x = cell_variable(cell, "x");
        auto cell_pos_y = cell_variable(cell, "y");
        prob.mut_variable_bound(cell_pos_x).mut_lower_bound() =
            max(cell->rect().cx() - cell_move_bound,
                double(core.lx() + cell->rect().dx() / 2)) * unit / netlength_scaling_unit;
        prob.mut_variable_bound(cell_pos_x).mut_upper_bound() =
            min(cell->rect().cx() + cell_move_bound,
                double(core.ux() - cell->rect().dx() / 2)) * unit / netlength_scaling_unit;
        prob.mut_variable_bound(cell_pos_y).mut_lower_bound() =
            max(cell->rect().cy() - cell_move_bound,
                double(core.ly() + cell->rect().dy() / 2)) * unit / netlength_scaling_unit;
        prob.mut_variable_bound(cell_pos_y).mut_upper_bound() =
            min(cell->rect().cy() + cell_move_bound,
                double(core.uy() - cell->rect().dy() / 2)) * unit / netlength_scaling_unit;
    }

    for (auto net : movable_nets) {
        auto net_lx = net_variable(net, "lx");
        auto net_ux = net_variable(net, "ux");
        auto net_ly = net_variable(net, "ly");
        auto net_uy = net_variable(net, "uy");

        int64_t min_lx = core.ux();
        int64_t max_ux = core.lx();
        int64_t min_ly = core.uy();
        int64_t max_uy = core.ly();
        for (auto& p : net->pins()) {
            if (p.get().term_type() == Pin::TERM_TYPE::ODB_BTERM) {
                auto x = p.get().c(X);
                auto y = p.get().c(Y);
                if (x < min_lx) {
                    min_lx = x;
                    prob.mut_variable_bound(net_lx).mut_upper_bound() = x * unit / netlength_scaling_unit;
                }
                if (x > max_ux) {
                    max_ux = x;
                    prob.mut_variable_bound(net_ux).mut_lower_bound() = x * unit / netlength_scaling_unit;
                }
                if (y < min_ly) {
                    min_ly = y;
                    prob.mut_variable_bound(net_ly).mut_upper_bound() = y * unit / netlength_scaling_unit;
                }
                if (y > max_uy) {
                    max_uy = y;
                    prob.mut_variable_bound(net_uy).mut_lower_bound() = y * unit / netlength_scaling_unit;
                }
            } else {
                FASSERT(p.get().term_type() == Pin::TERM_TYPE::ODB_ITERM);
                auto cell = &(p.get().cell()->get());
                if (movable_cells.contains(cell)) {
                    auto cell_pos_x = cell_variable(cell, "x");
                    auto cell_pos_y = cell_variable(cell, "y");
                    auto pin_offset_x = p.get().offset(X) * unit / netlength_scaling_unit;
                    auto pin_offset_y = p.get().offset(Y) * unit / netlength_scaling_unit;
                    for (auto& tuple :
                        {make_tuple(net_lx, cell_pos_x, pin_offset_x),
                        make_tuple(cell_pos_x, net_ux, -pin_offset_x),
                        make_tuple(net_ly, cell_pos_y, pin_offset_y),
                        make_tuple(cell_pos_y, net_uy, -pin_offset_y)})
                    {
                        auto cons = prob.add_constraint();
                        cons.mut_upper_bound() = get<2>(tuple);
                        cons[get<0>(tuple)] = 1.0;
                        cons[get<1>(tuple)] = -1.0;
                    }
                } else {
                    auto x = p.get().c(X);
                    auto y = p.get().c(Y);
                    if (x < min_lx) {
                        min_lx = x;
                        prob.mut_variable_bound(net_lx).mut_upper_bound() = x * unit / netlength_scaling_unit;
                    }
                    if (x > max_ux) {
                        max_ux = x;
                        prob.mut_variable_bound(net_ux).mut_lower_bound() = x * unit / netlength_scaling_unit;
                    }
                    if (y < min_ly) {
                        min_ly = y;
                        prob.mut_variable_bound(net_ly).mut_upper_bound() = y * unit / netlength_scaling_unit;
                    }
                    if (y > max_uy) {
                        max_uy = y;
                        prob.mut_variable_bound(net_uy).mut_lower_bound() = y * unit / netlength_scaling_unit;
                    }
                }
            }
        }
    }

    for (auto net : movable_nets) {
        auto old_wl = hpwl(net) * unit / netlength_scaling_unit;
        auto cons = prob.add_constraint();
        cons.mut_lower_bound() = old_wl;
        cons.mut_upper_bound() = old_wl;
        cons[net_variable(net, "ux")] = 1.0;
        cons[net_variable(net, "lx")] = -1.0;
        cons[net_variable(net, "uy")] = 1.0;
        cons[net_variable(net, "ly")] = -1.0;
        cons[net_variable(net, "delta_netlength")] = -1.0;

        auto var = net_variable(net, "abs_delta_netlength");
        auto cons2 = prob.add_constraint();
        cons2.mut_lower_bound() = 0.0;
        cons2[var] = 1.0;
        cons2[net_variable(net, "delta_netlength")] = -1.0;
        auto cons3 = prob.add_constraint();
        cons3.mut_lower_bound() = 0.0;
        cons3[var] = 1.0;
        cons3[net_variable(net, "delta_netlength")] = 1.0;
    }
}

void add_seglength_constraints(
    Problem<string>& prob,
    shared_ptr<Database> linden_db,
    const unordered_set<const Cell*>& movable_cells,
    const unordered_set<const Net*>& movable_nets,
    const deque<Segment>& segments)
{
    auto unit = length_unit * time_unit;
    auto core = linden_db->core_region();
    for (auto& seg : segments) {
        auto net = &(seg.first->net());
        if (!movable_nets.contains(net)) {
            continue;
        }
        if (net->pins().size() == 2) {
            continue;
        }
        auto seg_lx = seg_variable(seg, "lx");
        auto seg_ux = seg_variable(seg, "ux");
        auto seg_ly = seg_variable(seg, "ly");
        auto seg_uy = seg_variable(seg, "uy");

        int64_t min_lx = core.ux();
        int64_t max_ux = core.lx();
        int64_t min_ly = core.uy();
        int64_t max_uy = core.ly();
        for (auto pin : {seg.first, seg.second}) {
            if (pin->term_type() == Pin::TERM_TYPE::ODB_BTERM) {
                auto x = pin->c(X);
                auto y = pin->c(Y);
                if (x < min_lx) {
                    min_lx = x;
                    prob.mut_variable_bound(seg_lx).mut_upper_bound() = x * unit / netlength_scaling_unit;
                }
                if (x > max_ux) {
                    max_ux = x;
                    prob.mut_variable_bound(seg_ux).mut_lower_bound() = x * unit / netlength_scaling_unit;
                }
                if (y < min_ly) {
                    min_ly = y;
                    prob.mut_variable_bound(seg_ly).mut_upper_bound() = y * unit / netlength_scaling_unit;
                }
                if (y > max_uy) {
                    max_uy = y;
                    prob.mut_variable_bound(seg_uy).mut_lower_bound() = y * unit / netlength_scaling_unit;
                }
            } else {
                FASSERT(pin->term_type() == Pin::TERM_TYPE::ODB_ITERM);
                auto cell = &(pin->cell()->get());
                if (movable_cells.contains(cell)) {
                    auto cell_pos_x = cell_variable(cell, "x");
                    auto cell_pos_y = cell_variable(cell, "y");
                    auto pin_offset_x = pin->offset(X) * unit / netlength_scaling_unit;
                    auto pin_offset_y = pin->offset(Y) * unit / netlength_scaling_unit;
                    for (auto& tuple :
                        {make_tuple(seg_lx, cell_pos_x, pin_offset_x),
                        make_tuple(cell_pos_x, seg_ux, -pin_offset_x),
                        make_tuple(seg_ly, cell_pos_y, pin_offset_y),
                        make_tuple(cell_pos_y, seg_uy, -pin_offset_y)})
                    {
                        auto cons = prob.add_constraint();
                        cons.mut_upper_bound() = get<2>(tuple);
                        cons[get<0>(tuple)] = 1.0;
                        cons[get<1>(tuple)] = -1.0;
                    }
                } else {
                    auto x = pin->c(X);
                    auto y = pin->c(Y);
                    if (x < min_lx) {
                        min_lx = x;
                        prob.mut_variable_bound(seg_lx).mut_upper_bound() = x * unit / netlength_scaling_unit;
                    }
                    if (x > max_ux) {
                        max_ux = x;
                        prob.mut_variable_bound(seg_ux).mut_lower_bound() = x * unit / netlength_scaling_unit;
                    }
                    if (y < min_ly) {
                        min_ly = y;
                        prob.mut_variable_bound(seg_ly).mut_upper_bound() = y * unit / netlength_scaling_unit;
                    }
                    if (y > max_uy) {
                        max_uy = y;
                        prob.mut_variable_bound(seg_uy).mut_lower_bound() = y * unit / netlength_scaling_unit;
                    }
                }
            }
        }
    }

    for (auto& seg : segments) {
        auto net = &(seg.first->net());
        if (movable_nets.contains(net)) {
            if (net->pins().size() == 2) {
                auto cons = prob.add_constraint();
                cons.mut_lower_bound() = 0.0;
                cons.mut_upper_bound() = 0.0;
                cons[seg_variable(seg, "delta_seglength")] = 1.0;
                cons[net_variable(net, "delta_netlength")] = -1.0;
                continue;
            }
            auto old_seg_wl = find_seg_WL(seg) * unit;
            auto cons = prob.add_constraint();
            cons.mut_lower_bound() = old_seg_wl;
            cons.mut_upper_bound() = old_seg_wl;
            cons[seg_variable(seg, "ux")] = 1.0;
            cons[seg_variable(seg, "lx")] = -1.0;
            cons[seg_variable(seg, "uy")] = 1.0;
            cons[seg_variable(seg, "ly")] = -1.0;
            cons[seg_variable(seg, "delta_seglength")] = -1.0;
        }
    }
}

void add_timing_arc_constraints(
    Problem<string>& prob,
    const deque<TimingArc>& timing_arcs,
    double wire_cap)
{
    FASSERT(wire_cap > 0) (wire_cap);

    // add constraint for cload of driver pins
    unordered_set<const Pin*> outpin_record;
    for (auto& arc : timing_arcs) {
        auto outpin = arc.output_pin;
        if (outpin_record.contains(outpin)) {
            continue;
        }
        outpin_record.insert(outpin);

        const Net* net = &(outpin->net());
        auto var = pin_variable(outpin, "delta_cload");
        auto cons = prob.add_constraint();
        cons.mut_lower_bound() = 0.0;
        cons.mut_upper_bound() = 0.0;
        cons[var] = 1.0 ;
        cons[net_variable(net, "delta_netlength")] = -wire_cap * netlength_scaling_unit * cload_scaling_unit;

        // ignore this constraint for cmax, which may result in no solution
        // add upper bound for delta_cload
        // auto old_wl = hp(net);
        // prob.mut_variable_bound(var).mut_upper_bound() =
        //     arc.cmax - arc.cpins - wire_cap * old_wl;
    }

    unordered_map<const Pin*, double> outpin2max_inslope;
    for (auto& arc : timing_arcs) {
        auto outpin = arc.output_pin;
        auto inslope = arc.input_slew;
        if (!outpin2max_inslope.contains(outpin)) {
            outpin2max_inslope.insert({outpin, inslope});
        } else if (inslope > outpin2max_inslope.at(outpin)) {
            outpin2max_inslope.at(outpin) = inslope;
        }
    }
    // add constraint for arc delay and outslope
    for (auto& arc : timing_arcs) {
        auto inpin = arc.input_pin;
        auto outpin = arc.output_pin;
        // note the order is reversed in the data structure given by Zekun
        // A1 is small, A2 is large
        // indeed it makes sense that the effect of transition decays in propogation
        auto A1 = arc.delay_args[1];
        auto A2 = arc.delay_args[0];
        auto B1 = arc.transition_args[1];
        auto B2 = arc.transition_args[0];

        // arc delay
        auto cons = prob.add_constraint();
        cons.mut_lower_bound() = 0.0;
        cons.mut_upper_bound() = 0.0;
        cons[arc_variable(arc, "delta_delay")] = 1.0;
        cons[pin_variable(outpin, "delta_cload")] = -A1 / cload_scaling_unit;
        if(fabs(A2)>1e-2){
           cons[pin_variable(inpin, "delta_inslope")] = -A2;
        }


        // lower bound on delta arc delay (should not make delay < 0)
        FASSERT(arc.arc_delay >= 0.0) (arc.arc_delay);
        // prob.mut_variable_bound(arc_variable(arc, "delta_delay")).mut_lower_bound() =
        //     -arc.arc_delay * time_unit;

        // outslope
        // Remark: outslope of an output pin is the maximum outslope among
        // all timing arcs with this output pin.
        auto inslope = arc.input_slew;
        auto max_inslope = outpin2max_inslope.at(outpin);
        auto cons2 = prob.add_constraint();
        cons2.mut_lower_bound() = B2 * (inslope - max_inslope) * time_unit;
        cons2[pin_variable(outpin, "delta_outslope")] = 1.0;
        cons2[pin_variable(outpin, "delta_cload")] = -B1 / cload_scaling_unit;
        if(fabs(B2)>1e-2){
            cons2[pin_variable(inpin, "delta_inslope")] = -B2;
        }
    }
}

void add_net_constraints(
    Problem<string>& prob,
    const unordered_set<const Net*>& movable_nets,
    const deque<TimingArc>& timing_arcs,
    const deque<Port>& ports,
    const deque<Segment>& segments,
    double wire_cap,
    double wire_res,
    double delta_wl_bound,
    bool wire_delay)
{
    FASSERT(wire_res > 0.0) (wire_res);
    delta_wl_bound *= length_unit;

    if (!wire_delay) {
        // if wire_delay is ignored
        for (auto& seg : segments) {
            // inslope
            auto seg_in = seg.first;    // output of the previous timing arc
            auto seg_out = seg.second;    // input of the next timing arc
            // skip if seg_out is port
            if (seg_out->term_type() == Pin::TERM_TYPE::ODB_BTERM) {
                continue;
            }

            // no delay
            auto delay_cons = prob.add_constraint();
            delay_cons.mut_lower_bound() = 0.0;
            delay_cons.mut_upper_bound() = 0.0;
            delay_cons[seg_variable(seg, "delta_delay")] = 1.0;

            // no slew propagation
            auto inslope_cons = prob.add_constraint();
            inslope_cons.mut_lower_bound() = 0.0;
            inslope_cons.mut_upper_bound() = 0.0;
            inslope_cons[pin_variable(seg_out, "delta_inslope")] = 1.0;
            inslope_cons[pin_variable(seg_in, "delta_outslope")] = -1.0;
        }
        return;
    }

    // if wire_delay is considered
    for (auto& seg : segments) {
        auto net = &(seg.first->net());
        if (movable_nets.contains(net)) {
            // delta_seglength squared
            for (size_t i = 0; i < 9; ++i) {
                auto C1 = 3 * delta_wl_bound * std::pow(0.5, i + 1);
                auto C2 = delta_wl_bound * delta_wl_bound * std::pow(0.5, 2 * i + 1);
                C2 *= time_unit;
                auto cons1 = prob.add_constraint();
                cons1.mut_lower_bound() = -C2;
                cons1[seg_variable(seg, "sq_delta_seglength")] = 1.0;
                cons1[seg_variable(seg, "delta_seglength")] = C1;
                auto cons2 = prob.add_constraint();
                cons2.mut_lower_bound() = -C2;
                cons2[seg_variable(seg, "sq_delta_seglength")] = 1.0;
                cons2[seg_variable(seg, "delta_seglength")] = -C1;
            }
            auto C3 = delta_wl_bound * std::pow(0.5, 9);
            auto cons1 = prob.add_constraint();
            cons1.mut_lower_bound() = 0.0;
            cons1[seg_variable(seg, "sq_delta_seglength")] = 1.0;
            cons1[seg_variable(seg, "delta_seglength")] = C3;
            auto cons2 = prob.add_constraint();
            cons2.mut_lower_bound() = 0.0;
            cons2[seg_variable(seg, "sq_delta_seglength")] = 1.0;
            cons2[seg_variable(seg, "delta_seglength")] = -C3;
        }
    }

    // prepare cpins for each segment driver
    unordered_map<const Pin*, double> pin2cpins;
    for (auto& arc : timing_arcs) {
        pin2cpins[arc.output_pin] = arc.cpins;
    }
    for (auto& port : ports) {
        if (port.is_input) {
            FASSERT(port.cpins.has_value());
            pin2cpins[port.bterm] = *port.cpins;
        }
    }

    // wire delay and inslope of pins
    for (auto& seg : segments) {
        // delta delay
        auto old_seg_wl = find_seg_WL(seg) * length_unit;
        auto cpins = pin2cpins[seg.first];
        auto delay_cons = prob.add_constraint();
        delay_cons.mut_lower_bound() = 0.0;
        delay_cons.mut_upper_bound() = 0.0;
        delay_cons[seg_variable(seg, "delta_delay")] = 1.0;
        delay_cons[seg_variable(seg, "delta_seglength")] =
            -K_D * wire_res * (wire_cap * old_seg_wl + cpins);
        delay_cons[seg_variable(seg, "sq_delta_seglength")] =
            -K_D * wire_res * wire_cap / 2;

        // inslope
        auto seg_in = seg.first;    // output of the previous timing arc
        auto seg_out = seg.second;    // input of the next timing arc
        // skip if seg_out is port
        if (seg_out->term_type() == Pin::TERM_TYPE::ODB_BTERM) {
            continue;
        }
        auto inslope_cons = prob.add_constraint();
        inslope_cons.mut_lower_bound() = 0.0;
        inslope_cons.mut_upper_bound() = 0.0;
        inslope_cons[pin_variable(seg_out, "delta_inslope")] = 1.0;
        inslope_cons[seg_variable(seg, "delta_seglength")] =
            -K_S * wire_res * (wire_cap * old_seg_wl + cpins);
        inslope_cons[seg_variable(seg, "sq_delta_seglength")] =
            -K_S * wire_res * wire_cap / 2;
        inslope_cons[pin_variable(seg_in, "delta_outslope")] = -1.0;
    }
}

void add_arrival_time_constraints(
    Problem<string>& prob,
    const deque<TimingArc>& timing_arcs,
    const deque<Segment>& segments)
{
    // part 1. iterate through each segment
    for (auto& seg : segments) {
        auto seg_in = seg.first;    // output of the previous timing arc
        auto seg_out = seg.second;  // input of the next timing arc
        auto cons = prob.add_constraint();
        cons.mut_lower_bound() = 0.0;
        cons.mut_upper_bound() = 0.0;
        cons[pin_variable(seg_out, "delta_arrival")] = 1.0;
        cons[pin_variable(seg_in, "delta_arrival")] = -1.0;
        cons[seg_variable(seg, "delta_delay")] = -1.0;
    }

    // part 2. iterate through each timing arc
    for (auto& arc : timing_arcs) {
        auto inpin = arc.input_pin;
        auto outpin = arc.output_pin;
        auto cons = prob.add_constraint();
        double lb = arc.input_arr + arc.arc_delay - arc.output_arr;
        lb = min(lb, 0.0);  // lb to be fixed: two modes of arrival times
        cons.mut_lower_bound() = lb * time_unit;
        cons[pin_variable(outpin, "delta_arrival")] = 1.0;
        cons[pin_variable(inpin, "delta_arrival")] = -1.0;
        cons[arc_variable(arc, "delta_delay")] = -1.0;
    }
}

void add_sequantial_constraints(
    Problem<string>& prob,
    const deque<SeqPin>& seq_pins,
    const unordered_set<const Pin*>& end_points)
{
    // for all endpoints
    for (auto& seq_pin : seq_pins) {
        auto pin = seq_pin.pin;
        if (!end_points.contains(pin)) {
            continue;
        }
        auto cons = prob.add_constraint();
        cons.mut_lower_bound() = 0.0;
        cons.mut_upper_bound() = 0.0;
        cons[pin_variable(pin, "delta_required")] = 1.0;
        FASSERT(seq_pin.setup_arg.has_value());
        if(fabs(*seq_pin.setup_arg)>1e-2){
            cons[pin_variable(pin, "delta_inslope")] = (*seq_pin.setup_arg);
        }
        // need to bound delta_inslope here from below to ensure that
        // the lp problem is bounded
        auto inslope = seq_pin.slew;
        if (*seq_pin.setup_arg >= 0) {
            prob.mut_variable_bound(pin_variable(pin, "delta_inslope")).mut_lower_bound() =
            -abs(inslope) * time_unit;
        } else {
            prob.mut_variable_bound(pin_variable(pin, "delta_inslope")).mut_upper_bound() =
            abs(inslope) * time_unit;
        }
    }
}

void add_boundary_constraints(
    Problem<string>& prob,
    const deque<TimingArc>& timing_arcs,
    const deque<SeqPin>& seq_pins,
    const deque<Port>& ports,
    const unordered_set<const Pin*>& start_points,
    const unordered_set<const Pin*>& end_points,
    double wire_cap)
{
    // combinational logic cell startpoints
    for (auto& arc : timing_arcs) {
        auto inpin = arc.input_pin;
        auto outpin = arc.output_pin;
        if (!start_points.contains(inpin)) {
            continue;
        }
        auto B1 = arc.transition_args[1];
        auto var1 = pin_variable(outpin, "delta_outslope");
        const Net* net = &(outpin->net());
        auto var2 = net_variable(net, "delta_netlength");
        auto cons = prob.add_constraint();
        cons.mut_lower_bound() = 0.0;
        cons.mut_upper_bound() = 0.0;
        cons[var1] = 1.0;
        cons[var2] = -wire_cap * B1 * netlength_scaling_unit;

        auto A1 = arc.delay_args[1];
        auto var3 = pin_variable(inpin, "delta_arrival");
        auto cons2 = prob.add_constraint();
        cons2.mut_lower_bound() = 0.0;
        cons2.mut_upper_bound() = 0.0;
        cons2[var3] = 1.0;
        cons2[var2] = -wire_cap * A1 * netlength_scaling_unit;
    }

    // sequential cell startpoints
    for (auto& seq_pin : seq_pins) {
        if (!start_points.contains(seq_pin.pin)) {
            continue;
        }
        auto pin = seq_pin.pin;
        auto var1 = pin_variable(pin, "delta_inslope");
        prob.mut_variable_bound(var1).mut_lower_bound() = 0.0;
        prob.mut_variable_bound(var1).mut_upper_bound() = 0.0;
        auto var2 = pin_variable(pin, "delta_arrival");
        prob.mut_variable_bound(var2).mut_lower_bound() = 0.0;
        prob.mut_variable_bound(var2).mut_upper_bound() = 0.0;
    }
    // port startpoints
    for (auto& port : ports) {
        if (!start_points.contains(port.bterm)) {
            continue;
        }
        auto pin = port.bterm;
        auto var1 = pin_variable(pin, "delta_outslope");
        prob.mut_variable_bound(var1).mut_lower_bound() = 0.0;
        prob.mut_variable_bound(var1).mut_upper_bound() = 0.0;
        auto var2 = pin_variable(pin, "delta_arrival");
        prob.mut_variable_bound(var2).mut_lower_bound() = 0.0;
        prob.mut_variable_bound(var2).mut_upper_bound() = 0.0;
    }

    // combinational logic cell endpoints
    for (auto& arc : timing_arcs) {
        auto pin = arc.output_pin;
        if (!end_points.contains(pin)) {
            continue;
        }
        auto cons = prob.add_constraint();
        cons.mut_lower_bound() = 0.0;
        cons.mut_upper_bound() = 0.0;
        cons[pin_variable(pin, "delta_slack")] = 1.0;
        cons[pin_variable(pin, "delta_arrival")] = 1.0;
    }

    // sequential cell endpoints
    for (auto& seq_pin : seq_pins) {
        auto pin = seq_pin.pin;
        if (!end_points.contains(pin)) {
            continue;
        }
        auto cons = prob.add_constraint();
        cons.mut_lower_bound() = 0.0;
        cons.mut_upper_bound() = 0.0;
        cons[pin_variable(pin, "delta_slack")] = 1.0;
        cons[pin_variable(pin, "delta_required")] = -1.0;
        cons[pin_variable(pin, "delta_arrival")] = 1.0;
    }

    // port endpoints
    for (auto& port : ports) {
        auto pin = port.bterm;
        if (!end_points.contains(pin)) {
            continue;
        }
        auto cons = prob.add_constraint();
        cons.mut_lower_bound() = 0.0;
        cons.mut_upper_bound() = 0.0;
        cons[pin_variable(pin, "delta_slack")] = 1.0;
        cons[pin_variable(pin, "delta_arrival")] = 1.0;
    }
}

void add_timing_metrics_constraints(
    Problem<string>& prob,
    const unordered_set<const Net*>& movable_nets,
    const deque<TimingArc>& timing_arcs,
    const deque<SeqPin>& seq_pins,
    const deque<Port>& ports,
    const unordered_set<const Pin*>& end_points,
    bool add_wns,
    bool add_tns)
{
    FASSERT(add_wns || add_tns);
    double wns_unchanged_ends = DBL_MAX;    // ends not in the subcircuit
    double old_wns = DBL_MAX;

    // objective
    auto obj = prob.max_objective();

    double weight_for_wns = 1.0;
    double weight_for_tns = 0.5  / end_points.size();
    double weight_for_delta_netlength = -1.0;

    if (add_wns) {
        obj["WNS"] = weight_for_wns;
    }
    if (add_tns) {
        obj["delta_TNS"] = weight_for_tns;
    }

    auto tns_cons = prob.add_constraint();
    tns_cons.mut_lower_bound() = 0.0;
    tns_cons.mut_upper_bound() = 0.0;
    tns_cons["delta_TNS"] = 1.0;
    // add abs_delta_netlength in obj to penalize netlength change
    for (auto net : movable_nets) {
        obj[net_variable(net, "abs_delta_netlength")] = weight_for_delta_netlength;
    }

    // combinational logic cell endpoints
    unordered_set<const Pin*> outpin_record;
    for (auto& arc : timing_arcs) {
        auto pin = arc.output_pin;
        if (!end_points.contains(pin)) {
            continue;
        }
        if (outpin_record.contains(pin)) {
            continue;
        }
        outpin_record.insert(pin);

        auto old_slack = arc.output_slack * time_unit;

        // wns
        auto cons = prob.add_constraint();
        cons.mut_upper_bound() = old_slack;
        cons["WNS"] = 1.0;
        cons[pin_variable(pin, "delta_slack")] = -1.0;

        // tns
        auto var = pin_variable(pin, "delta_neg_slack");
        auto cons2 = prob.add_constraint();
        cons2[var] = 1.0;
        cons2[pin_variable(pin, "delta_slack")] = -1.0;
        if (old_slack < 0) {
            prob.mut_variable_bound(var).mut_upper_bound() = -old_slack;
            cons2.mut_upper_bound() = 0.0;
        } else {
            prob.mut_variable_bound(var).mut_upper_bound() = 0.0;
            cons2.mut_upper_bound() = old_slack;
        }

        tns_cons[var] = -1.0;
    }

    // sequential cell endpoints
    for (auto& seq_pin : seq_pins) {
        if (!seq_pin.is_end) {
            continue;
        }
        auto pin = seq_pin.pin;
        auto old_slack = seq_pin.slack * time_unit;
        if (!end_points.contains(pin)) {
            if (old_slack < wns_unchanged_ends) {
                wns_unchanged_ends = old_slack;
            }
            continue;
        }
        if (old_slack < old_wns) {
            old_wns = old_slack;
        }
        // wns
        auto cons = prob.add_constraint();
        cons.mut_upper_bound() = old_slack;
        cons["WNS"] = 1.0;
        cons[pin_variable(pin, "delta_slack")] = -1.0;

        // tns
        auto var = pin_variable(pin, "delta_neg_slack");
        auto cons2 = prob.add_constraint();
        cons2[var] = 1.0;
        cons2[pin_variable(pin, "delta_slack")] = -1.0;
        if (old_slack < 0) {
            prob.mut_variable_bound(var).mut_upper_bound() = -old_slack;
            cons2.mut_upper_bound() = 0.0;
        } else {
            prob.mut_variable_bound(var).mut_upper_bound() = 0.0;
            cons2.mut_upper_bound() = old_slack;
        }

        tns_cons[var] = -1.0;
    }

    // port endpoints
    for (auto& port : ports) {
        if (!port.is_output) {
            continue;
        }
        auto pin = port.bterm;
        FASSERT(port.output_slack.has_value());
        auto old_slack = *port.output_slack * time_unit;
        if (!end_points.contains(pin)) {
            if (old_slack < wns_unchanged_ends) {
                wns_unchanged_ends = old_slack;
            }
            continue;
        }
        if (old_slack < old_wns) {
            old_wns = old_slack;
        }
        // wns
        auto cons = prob.add_constraint();
        cons.mut_upper_bound() = old_slack;
        cons["WNS"] = 1.0;
        cons[pin_variable(pin, "delta_slack")] = -1.0;

        // tns
        auto var = pin_variable(pin, "delta_neg_slack");
        prob.mut_variable_bound(var).mut_upper_bound() = max(-old_slack, 0.0);
        auto cons2 = prob.add_constraint();
        cons2.mut_upper_bound() = 0.0;
        cons2[var] = 1.0;
        cons2[pin_variable(pin, "delta_slack")] = -1.0;

        tns_cons[var] = -1.0;
    }

    // TBD: wns can be fixed better than max(old_wns,0), upper_bound may not be set
    double wns_upper_bound = max(old_wns, 0.0);
    prob.mut_variable_bound("WNS").mut_upper_bound() = wns_upper_bound;
    spdlog::info("WNS of unchanged ends: {}", wns_unchanged_ends);
    spdlog::info("Old WNS of subcircuit: {}", old_wns);
}

// a function to check if each assumption is true
// assumption1: all nets have one driver
void assumption_checker(shared_ptr<Database> linden_db,
                        const deque<TimingArc>& timing_arcs,
                        const deque<Port>& ports)
{
    // assumption1
    // two types of drivers (1) a start point (input port)
    //                      (2) end of an arc (output pin)
    unordered_set<const Pin*> drivers;
    for (auto& arc : timing_arcs) {
        drivers.insert(arc.output_pin);
    }
    for (auto& port : ports) {
        if (port.is_input) {
            drivers.insert(port.bterm);
        }
    }

    for (auto& net : linden_db->nets()) {
        const size_t driver_cnt = std::count_if(
            net.pins().begin(), net.pins().end(), [&drivers](const auto& pin) {
                return drivers.contains(&pin.get());
            });
        if (driver_cnt > 1) {
            spdlog::error("net {} has {} drivers", net.name(), driver_cnt);
        }
    }
    spdlog::info("assumption_checker passed: all nets have one driver");
}

// subcircuit break point checker
// check: every point that is not an endpoint should have an edge (arc / seg)
// pointing outwards
// This makes sure that there is no break point in any path
void subcircuit_break_checker(const Subcircuit& circuit) {
    auto& start_points = circuit.start_points;
    auto& end_points = circuit.end_points;
    auto& timing_arcs = circuit.timing_arcs;
    auto& segments = circuit.segments;

    // preparation: pin2arc, pin2seg
    auto pin2arc = [&]() {
        unordered_map<const Pin*, deque<const TimingArc*>> pin2arc;
        for (auto& arc : timing_arcs) {
            auto inpin = arc.input_pin;
            if (!pin2arc.contains(inpin)) {
                pin2arc.insert({inpin, {&arc}});
            } else {
                pin2arc[inpin].emplace_back(&arc);
            }
        }
        return pin2arc;
    }();
    auto pin2seg = [&]() {
        unordered_map<const Pin*, deque<const Segment*>> pin2seg;
        for (const auto& seg : segments) {
            auto inpin = seg.first;
            if (!pin2seg.contains(inpin)) {
                pin2seg.insert({inpin, {&seg}});
            } else {
                pin2seg[inpin].emplace_back(&seg);
            }
        }
        return pin2seg;
    }();

    // collect all pins, excluding the endpoints
    auto all_pins = [&]() {
        unordered_set<const Pin*> all_pins = start_points;
        for (const auto& arc : timing_arcs) {
            all_pins.insert(arc.input_pin);
            if (!end_points.contains(arc.output_pin)) {
                all_pins.insert(arc.output_pin);
            }
        }
        for (const auto& seg : segments) {
            all_pins.insert(seg.first);
            if (!end_points.contains(seg.second)) {
                all_pins.insert(seg.second);
            }
        }
        return all_pins;
    }();

    for (const auto& pin : all_pins) {
        if (!pin2arc.contains(pin) && !pin2seg.contains(pin)) {
            spdlog::error("subcircuit_break_checker failed: Break Pin: {}", get_pin_varname(pin));
        }
    }
    spdlog::info("subcircuit_break_checker Passed");
}

// circuit decomposition checker
// check: the disjoint union should be the original circuit
void decompose_checker(
    const Subcircuit& large_circuit,
    const deque<Subcircuit>& small_circuits)
{
    auto& large_start_points = large_circuit.start_points;
    auto& large_end_points = large_circuit.end_points;
    auto& large_arcs = large_circuit.timing_arcs;
    auto& large_segs = large_circuit.segments;

    auto check_start_end_points = [&]() {
        for (auto& circuit : small_circuits) {
            auto& small_start_points = circuit.start_points;
            auto& small_end_points = circuit.end_points;

            for (const auto& p : small_start_points) {
                if (!large_start_points.contains(p)) {
                    spdlog::error("Decompose_checker: Incorrect start points");
                }
            }
            for (const auto& p : small_end_points) {
                if (!large_end_points.contains(p)) {
                    spdlog::error("Decompose_checker: Incorrect end points");
                }
            }
        }
    };
    check_start_end_points();

    auto check_start_end_points_number = [&]() {
        size_t n1 = 0;
        size_t n2 = 0;
        for (auto& circuit : small_circuits) {
            n1 += circuit.start_points.size();
            n2 += circuit.end_points.size();
        }
        if (n1 != large_start_points.size()) {
            spdlog::error("Decompose_checker: mismatch on number of start points:"
            "large_start_points={}, sum_small_start_points{}", large_start_points.size(), n1);
        }
        if (n2 != large_end_points.size()) {
            spdlog::error("Decompose_checker: mismatch on number of end points:"
            "large_end_points={}, sum_small_end_points{}", large_end_points.size(), n2);
        }
    };
    check_start_end_points_number();

    auto check_arcs = [&]() {
        for (auto& circuit : small_circuits) {
            for (auto& arc : circuit.timing_arcs) {
                auto inpin = arc.input_pin;
                auto outpin = arc.output_pin;
                bool found = false;
                for (auto arc2 : large_arcs) {
                    auto inpin2 = arc2.input_pin;
                    auto outpin2 = arc2.output_pin;
                    if (equal_pins(inpin, inpin2) && equal_pins(outpin, outpin2)) {
                        found = true;
                    }
                }
                if (!found) {
                    spdlog::error("Decompose_checker: Incorrect timing arcs\n",
                                  "Fail to find Arc: {} ---> {}",
                                  get_pin_varname(inpin),
                                  get_pin_varname(outpin));
                }
            }
        }
    };
    check_arcs();

    auto check_segments = [&]() {
        for (auto& circuit : small_circuits) {
            for (auto& seg : circuit.segments) {
                auto inpin = seg.first;
                auto outpin = seg.second;
                bool found = false;
                for (auto seg2 : large_segs) {
                    auto inpin2 = seg2.first;
                    auto outpin2 = seg2.second;
                    if (equal_pins(inpin, inpin2) && equal_pins(outpin, outpin2)) {
                        found = true;
                    }
                }
                if (!found) {
                    spdlog::error("Decompose_checker: Incorrect segments\n",
                                  "Fail to find Segment: {} ---> {}",
                                  get_pin_varname(inpin),
                                  get_pin_varname(outpin));
                }
            }
        }
    };
    check_segments();

    auto check_timing_arcs_segs_number = [&]() {
        size_t n3 = 0;
        size_t n4 = 0;
        for (auto& circuit : small_circuits) {
            n3 += circuit.timing_arcs.size();
            n4 += circuit.segments.size();
        }
        if (n3 != large_arcs.size()) {
            spdlog::error("Decompose_checker: mismatch on number of timing arcs:"
                "large_arcs={}, sum_small_circuits_arcs={}", large_arcs.size(), n3);
        }
        if (n4 != large_segs.size()) {
            spdlog::error("Decompose_checker: mismatch on number of segments:"
                "large_segs={}, sum_small_circuits_segs={}", large_segs.size(), n4);
        }
    };
    check_timing_arcs_segs_number();

    spdlog::info("Decomposition Checker Passed");
}

// TBD: check whether the solution satisfies all constraints
void feasiblility_checker(
    shared_ptr<Database> linden_db,
    optional<unordered_map<string, double>> solution,
    const unordered_set<const Cell*>& movable_cells,
    const unordered_set<const Net*>& movable_nets)
{
    const auto unit = length_unit * time_unit;
    const auto& core = linden_db->core_region();
    auto get_pin_location = [&](const Pin& pin) -> Point {
        int64_t x = 0;
        int64_t y = 0;
        if (pin.term_type() == Pin::TERM_TYPE::ODB_BTERM) {
            x = pin.c(X);
            y = pin.c(Y);
        } else {
            auto cell = &(pin.cell()->get());
            if (movable_cells.contains(cell)) {
                auto cell_pos_x = (*solution).at(cell_variable(cell, "x"));
                auto cell_pos_y = (*solution).at(cell_variable(cell, "y"));
                auto pin_offset_x = pin.offset(X);
                auto pin_offset_y = pin.offset(Y);
                x = cell_pos_x + pin_offset_x;
                y = cell_pos_y + pin_offset_y;
            } else {
                x = pin.c(X);
                y = pin.c(Y);
            }
        }
        return {x, y};
    };

    auto check_bbox_of_movable_net = [&](const Net* net) {
        int64_t min_lx = core.ux();
        int64_t max_ux = core.lx();
        int64_t min_ly = core.uy();
        int64_t max_uy = core.ly();
        for (auto& pin : net->pins()) {
            auto loc = get_pin_location(pin.get());
            if (loc.x() < min_lx) {
                min_lx = loc.x();
            }
            if (loc.x() > max_ux) {
                max_ux = loc.x();
            }
            if (loc.y() < min_ly) {
                min_ly = loc.y();
            }
            if (loc.y() > max_uy) {
                max_uy = loc.y();
            }
        }
        //FIXME: check the error here
    };

    auto sgn = [&](double x) -> int {
        double eps = lp_error;
        if (fabs(x) < eps)
        return 0;
        if (x < 0)
        return -1;
        else
        return 1;
    };

    auto check_delta_netlenght = [&](const Net* net) {
        auto net_lx = (*solution).at(net_variable(net, "lx"));
        auto net_ux = (*solution).at(net_variable(net, "ux"));
        auto net_ly = (*solution).at(net_variable(net, "ly"));
        auto net_uy = (*solution).at(net_variable(net, "uy"));
        // check delta_netlength and abs_delta_netlength
        auto old_wl = hpwl(net) * unit / netlength_scaling_unit ;
        auto new_wl = (net_ux - net_lx + net_uy - net_ly) * unit ;
        auto delta_netlength = (*solution).at(net_variable(net, "delta_netlength")) ;
        auto delta_netlength_error = abs(delta_netlength - (new_wl - old_wl)) ;
        if (sgn(delta_netlength_error) != 0) {
            spdlog::error("Feasiblility Error: delta_netlength_error: {} lp_error: {}",
                          delta_netlength_error, lp_error * unit);
        }
        auto abs_delta_netlength = (*solution).at(net_variable(net, "abs_delta_netlength"));
        auto abs_delta_netlength_error = abs(abs(delta_netlength) - abs_delta_netlength) ;
        if (sgn(abs_delta_netlength_error) != 0) {
            spdlog::error("Feasiblility Error: abs_delta_netlength_error: {} lp_error: {}",
                          abs_delta_netlength_error, lp_error * unit);
        }
    };

    for (auto net : movable_nets) {
        check_bbox_of_movable_net(net);
        check_delta_netlenght(net);
    }
    spdlog::info("Feasiblility Checker Passed");
}

// check the arrival time propagation
void arrival_time_checker(
    optional<unordered_map<string, double>> solution,
    const Subcircuit& circuit,
    const deque<Port>& ports,
    const deque<SeqPin>& seq_pins)
{
    const auto& timing_arcs = circuit.timing_arcs;
    const auto& segments = circuit.segments;
    const auto outpin2arc = [&]() {
        unordered_map<const Pin*, deque<const TimingArc*>> outpin2arc;
        for (auto& arc : timing_arcs) {
            auto outpin = arc.output_pin;
            if (!outpin2arc.contains(outpin)) {
                outpin2arc.insert({outpin, {&arc}});
            } else {
                outpin2arc.at(outpin).emplace_back(&arc);
            }
        }
        return outpin2arc;
    }();

    const auto outpin2seg = [&]() {
        unordered_map<const Pin*, deque<const Segment*>> outpin2seg;
        for (auto& seg : segments) {
            auto outpin = seg.second;
            if (!outpin2seg.contains(outpin)) {
                outpin2seg.insert({outpin, {&seg}});
            } else {
                outpin2seg.at(outpin).emplace_back(&seg);
            }
        }
        return outpin2seg;
    }();

    // check arrival time propagation of segs
    auto check_arrival_time_propagation_of_segs = [&]() {
        for (auto& pair : outpin2seg) {
            auto outpin = pair.first;
            auto out_delta_arrival
                = (*solution).at(pin_variable(outpin, "delta_arrival"));
            auto& seg_set = pair.second;
            for (auto seg : seg_set) {
                auto inpin = seg->first;
                auto in_delta_arrival =
                    (*solution).at(pin_variable(inpin, "delta_arrival"));
                auto seg_delta_delay =
                    (*solution).at(seg_variable(*seg, "delta_delay"));
                if (abs(in_delta_arrival + seg_delta_delay - out_delta_arrival) > lp_error) {
                  spdlog::error(
                      "Arrival Time Error: \n"
                      "Seg: {} ---> {} \n"
                      "Delta input arrival: {} \n"
                      "Delta out arrival: {} \n"
                      "Delta arc delay: {} \n",
                      get_pin_varname(inpin),
                      get_pin_varname(outpin),
                      in_delta_arrival,
                      out_delta_arrival,
                      seg_delta_delay);
                }
            }
        }
    };
    check_arrival_time_propagation_of_segs();

    // FIXME: check arrival time propagation of arcs
    // for (auto& pair : outpin2arc) {
    //     auto outpin = pair.first;
    //     auto out_delta_arrival =
    //         (*solution).at(pin_variable(outpin, "delta_arrival"));
    //     auto& arc_set = pair.second;
    //     FASSERT(!arc_set.empty());
    //     auto old_out_arrival = arc_set[0]->output_arr;
    //     for (auto arc : arc_set) {
    //         if (arc->output_arr > old_out_arrival) {
    //             old_out_arrival = arc->output_arr;
    //         }
    //     }
    //     auto out_arrival = old_out_arrival * time_unit + out_delta_arrival;

    //     for (auto arc : arc_set) {
    //         auto inpin = arc->input_pin;
    //         auto in_arrival = arc->input_arr * time_unit +
    //             (*solution).at(pin_variable(inpin, "delta_arrival"));
    //         auto arc_delay = arc->arc_delay * time_unit +
    //             (*solution).at(arc_variable(*arc, "delta_delay"));
    //         // an extra source of error coming from two modes of arrival times
    //         auto lb = (arc->input_arr + arc->arc_delay - arc->output_arr) * time_unit;
    //         if (out_arrival < in_arrival + arc_delay - lp_error - lb) {
    //           spdlog::info("Arrival Time Error: \n"
    //                         "Arc: {} ---> {} \n"
    //                         "Input arrival: {} \n"
    //                         "Out arrival: {} \n"
    //                         "Arc delay: {} \n"
    //                         "lb: {} \n",
    //                         get_pin_varname(inpin),
    //                         get_pin_varname(outpin),
    //                         in_arrival,
    //                         out_arrival,
    //                         arc_delay,
    //                         lb);
    //             return false;
    //         }
    //     }
    // }

    spdlog::info("Arrival Time Checker Passed");
}

// check whether it is better than doing nothing
void optimality_checker(
    optional<unordered_map<string, double>> solution,
    const deque<SeqPin>& seq_pins,
    const deque<Port>& ports,
    bool check_wns,
    bool check_tns)
{
    auto update_wns_tns = [](double& wns, double& tns, double slack) {
        slack *= time_unit;
        if (slack < wns) {
            wns = slack;
        }
        if (slack < 0) {
            tns += slack;
        }
    };
    double old_wns = DBL_MAX;
    double old_tns = 0.0;
    for (auto& seq_pin : seq_pins) {
        if (!seq_pin.is_end) {
            continue;
        }
        update_wns_tns(old_wns, old_tns, seq_pin.slack);
    }
    for (auto& port : ports) {
        if (!port.is_output) {
            continue;
        }
        update_wns_tns(old_wns, old_tns, *port.output_slack);
    }

    if (check_wns) {
        double new_wns = (*solution).at("WNS");
        spdlog::info("Old WNS = {} new estimated WNS of the changed part = {}", old_wns, new_wns);
        if (new_wns <= old_wns - lp_error) {
            spdlog::error("Optimality Error: WNS becomes worse");
        }
    }
    if (check_tns) {
        double new_tns = (*solution).at("delta_TNS") + old_tns;
        spdlog::info("Old TNS = {} new estimated TNS = {}", old_tns, new_tns);
        if (new_tns <= old_tns - lp_error) {
            spdlog::error("Optimality Error: TNS becomes worse");
        }
    }
    spdlog::info("Optimality Checker Passed");
}

// solve the lp problem for a sub-circuit
bool timing_driven_partial_move(
    shared_ptr<Database> linden_db,
    const deque<Port>& ports,
    const deque<SeqPin>& seq_pins,
    const Subcircuit& subcircuit,
    const TdpConfig config,
    int64_t& total_movement,
    double& wns_res)
{
    Problem<string> prob;
    add_netlength_constraints(prob, linden_db, subcircuit.movable_cells,
                                subcircuit.movable_nets,
                                config.cell_move_bound);
    add_timing_arc_constraints(prob, subcircuit.timing_arcs, config.wire_cap);
    add_net_constraints(prob, subcircuit.movable_nets,
                        subcircuit.timing_arcs, ports, subcircuit.segments,
                        config.wire_cap, config.wire_res, config.delta_wl_bound,
                        config.wire_delay);
    if (config.wire_delay) {
        add_seglength_constraints(prob, linden_db, subcircuit.movable_cells,
                                subcircuit.movable_nets,
                                subcircuit.segments);
    }
    add_arrival_time_constraints(prob, subcircuit.timing_arcs, subcircuit.segments);
    add_sequantial_constraints(prob, seq_pins, subcircuit.end_points);
    add_boundary_constraints(prob, subcircuit.timing_arcs, seq_pins, ports,
                            subcircuit.start_points, subcircuit.end_points,
                            config.wire_cap);
    add_timing_metrics_constraints(prob, subcircuit.movable_nets, subcircuit.timing_arcs,
                                    seq_pins, ports, subcircuit.end_points,
                                    config.wns, config.tns);
    // lp solve
    operations_research::MPSolverParameters params;
    params.SetDoubleParam(
        operations_research::MPSolverParameters::DoubleParam::PRIMAL_TOLERANCE,
        lp_error);
    params.SetDoubleParam(
        operations_research::MPSolverParameters::DoubleParam::DUAL_TOLERANCE,
        lp_error);

#ifndef NDEBUG
    auto spdlog_level = spdlog::get_level();
    spdlog::set_level(spdlog::level::debug);
    auto solution = prob.solve(params);
    spdlog::set_level(spdlog_level);
#else
    auto solution = prob.solve(params);
#endif

    if (!solution.has_value()) {
        return false;
    }

    // run checkers
    feasiblility_checker(linden_db, solution, subcircuit.movable_cells, subcircuit.movable_nets);
    optimality_checker(solution, seq_pins, ports, config.wns, config.tns);
    arrival_time_checker(solution, subcircuit, ports, seq_pins);

    // move cells
    double subcircuit_total_movement = 0.0;
    int64_t max_movement = 0;
    deque<tuple<const Cell*, int64_t, int64_t>> cells_with_large_movement;
    for (auto cell : subcircuit.movable_cells) {
        int64_t target_locs[2];
        int64_t m_distances[2];
        for (auto dim : {X, Y}) {
            auto old_loc = cell->rect().c(dim);
            auto cell_pos = cell_variable(cell, dim == X ? "x" : "y");
            auto new_loc = int64_t((*solution).at(cell_pos)*netlength_scaling_unit);
            target_locs[dim] = new_loc;
            m_distances[dim] = abs(new_loc - old_loc);
        }
        const_cast<Cell*>(cell)->move_to({target_locs[X], target_locs[Y]});
        auto movement = m_distances[X] + m_distances[Y];
        subcircuit_total_movement += movement;
        if (max_movement < movement) {
            max_movement = movement;
        }
        if (movement > 1e5) {
            cells_with_large_movement.emplace_back(make_tuple(cell, m_distances[X], m_distances[Y]));
        }
    }

    total_movement += subcircuit_total_movement;
    spdlog::info("Subcircuit average movement: {} nm", subcircuit_total_movement / subcircuit.movable_cells.size());
    spdlog::info("Subcircuit maximum movement: {} nm", max_movement);
    spdlog::info("Cells with large movement: ");
    for (auto& tuple : cells_with_large_movement) {
        spdlog::info("{} moves: ({}, {})", get<0>(tuple)->name(), get<1>(tuple), get<2>(tuple));
    }
    wns_res = (*solution).at("WNS");
    if (config.tns) {
        spdlog::info("delta_TNS: {}", (*solution).at("delta_TNS"));
    }
    return true;
}

void timing_driven_cell_move(
    shared_ptr<Database> linden_db,
    const deque<TimingArc>& timing_arcs,
    const deque<Port>& ports,
    const deque<SeqPin>& seq_pins,
    const TdpConfig config)
{
    const std::string dot_line = "---------------------------------------------------------";
    const std::string dot_2lines = dot_line + dot_line;

    // show slack info
    spdlog::info(dot_line);
    print_cell_slack_distribution(linden_db, timing_arcs, seq_pins);
    report_delays(linden_db, timing_arcs, ports, seq_pins);

    spdlog::info(dot_line);
    spdlog::info("wire_cap={}, wire_res={}", config.wire_cap, config.wire_res);
    spdlog::info(dot_line);
    
    // assumption check
    assumption_checker(linden_db, timing_arcs, ports);

    // select movable cells
    auto movable_cells = move(select_movable_cells(linden_db, timing_arcs, seq_pins));
    // extend_movable_cells(linden_db, movable_cells);
    auto movable_nets = move(find_movable_nets(linden_db, movable_cells, seq_pins));
    
    spdlog::info(dot_2lines);
    spdlog::info("Original Circuit: ");
    spdlog::info("num movable cells: {}", movable_cells.size());
    spdlog::info("num movable nets: {}", movable_nets.size());
    spdlog::info("num timing arcs: {}", timing_arcs.size());
    size_t n_start = std::count_if(ports.begin(), ports.end(), [](const auto& port) {
        return port.is_input;
    });
    n_start += std::count_if(seq_pins.begin(), seq_pins.end(), [](const auto& seqpin) {
        return seqpin.is_start;
    });
    size_t n_end = std::count_if(ports.begin(), ports.end(), [](const auto& port) {
        return port.is_output;
    });
    n_end += std::count_if(seq_pins.begin(), seq_pins.end(), [](const auto& seqpin) {
        return seqpin.is_end;
    });
    spdlog::info("num start points: {}", n_start);
    spdlog::info("num end points: {}", n_end);
    spdlog::info(dot_2lines);

    auto print_subcircuit_info = [](const std::string& desc, const Subcircuit& subcircuit) {
        spdlog::info("{}: ", desc);
        spdlog::info("num movable cells: {}", subcircuit.movable_cells.size());
        spdlog::info("num movable nets: {}", subcircuit.movable_nets.size());
        spdlog::info("num timing arcs: {}", subcircuit.timing_arcs.size());
        spdlog::info("num segs: {}", subcircuit.segments.size());
        spdlog::info("num start points: {}", subcircuit.start_points.size());
        spdlog::info("num end points: {}", subcircuit.end_points.size());
    };
    // shrink problem size by considering only a small part of the timing graph
    Subcircuit large_subcircuit;
    construct_effective_subcirsuit(linden_db, movable_cells, movable_nets, timing_arcs,
                        ports, seq_pins, large_subcircuit);
    print_subcircuit_info("Shrinked Circuit", large_subcircuit);
    spdlog::info(dot_2lines);

    deque<Subcircuit> small_subcircuits;
    if (decompose) {
        // decompose the shrinked circuit into smaller independent parts
        small_subcircuits = move(decompose_problem(linden_db, large_subcircuit,
                                                        ports, seq_pins));
        decompose_checker(large_subcircuit, small_subcircuits);
        spdlog::info("Decomposed into {} subcircuits", small_subcircuits.size());
    } else {
        small_subcircuits.emplace_back(large_subcircuit);
    }

    int64_t total_movement = 0;
    double wns_overall = DBL_MAX;
    size_t circuit_cnt = 1;
    for (auto& subcircuit : small_subcircuits) {
        if (decompose) {
            spdlog::info(dot_2lines);
            spdlog::info("TDP: subcircuit #{}", circuit_cnt);
            print_subcircuit_info("Subcircuit", subcircuit);
            if (subcircuit.end_points.empty()) {
                spdlog::error("No end points. Might be a clock net which should have been skipped in previous stage.");
            }
        }
        subcircuit_break_checker(subcircuit);
        spdlog::info("Solving LP ...");
        double wns = 0.0;
        bool solved = timing_driven_partial_move(linden_db,
                                                 ports,
                                                 seq_pins,
                                                 subcircuit,
                                                 config,
                                                 total_movement,
                                                 wns);
        if (wns < wns_overall) {
            wns_overall = wns;
        }
        if (solved) {
            spdlog::info("LP is solved successfully");
        } else {
            spdlog::info("Fail to solve LP: No valid move");
        }
        circuit_cnt++;
    }

    spdlog::info(dot_2lines);
    spdlog::info("Overall:");
    spdlog::info("Total Movement: {}", total_movement);
    spdlog::info("Average Movement: {}", 1.0 * total_movement / movable_cells.size());
    spdlog::info("New estimated WNS: {}", wns_overall);
    spdlog::info(dot_2lines);
}

}  // namespace tdp
}  // namespace linden
