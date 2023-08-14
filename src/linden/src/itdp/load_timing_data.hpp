#pragma once

#include "linden/db/db.hpp"

#include <unordered_map>
#include <cstdint>
#include <vector>
#include <iostream>

namespace linden {
namespace tdp {

struct TimingArc {
    const db::Pin* input_pin;
    const db::Pin* output_pin;
    double input_slack;
    double output_slack;
    double input_arr;
    double output_arr;
    double input_slew;
    double output_slew;
    double arc_delay;
    double cmax;               // the max capacitance that the output_pin can drive
    double cpins;              // the total capacitance of pins that the output_pin drives
    
    // NOTE: the order is swapped here because:
    //    in the lib file, the index_1 is ClockTransistion, and index_2 is OutputCapacitance
    //    in the paper, A1|B1 is the arg for Cap and A2|B2 is the arg for inslope(transition)
    // FIXME: need to assert the order in pbfile is expected, for different process nodes.
    double delay_args[2];      // [A2, A1] in the paper section 4.2.1
    double transition_args[2]; // [B2, B1] in the paper section 4.2.1
};

// Modeling The SequentialCell's Pins that can be the start or end point of a timing path
// Typically, the CLK Pin, D(Input) Pin.
struct SeqPin {
    const db::Pin* pin;
    bool is_start;
    bool is_end;

    // Note: Why we don't use S1 and S3 in the paper?
    //   We use idea clock at pre-cts stage, so no S3.
    //   the timing model of D pin is not related to cload, so no S1.
    std::optional<double> setup_arg;  // S2 in the paper section 4.2.4
    double slack;
    double arr;
    double slew;
};

struct Port {
    const db::BTerm* bterm;
    bool is_input;
    bool is_output;
    std::optional<double> cpins;
    std::optional<double> cmax;
    std::optional<double> output_slack;
    double arr;
    double slew;
};

struct TdpConfig {
    double wire_cap;         // unit capacitance of wire in paper
    double wire_res;         // unit resistance of wire in paper
    double cell_move_bound;  // M in paper section 4.1, limit of cell movement on both x and y dimension
    double delta_wl_bound;   // L in paper section 4.2.2, limit of wirelength change
    bool wire_delay;         // determines whether include wire_delay or not
    bool wns;                // add wns in obj
    bool tns;                // add tns in obj

    TdpConfig(double _wire_cap,
              double _wire_res,
              const linden::geom::Rect& core_region);
};

}  // namespace tdp
}  // namespace linden