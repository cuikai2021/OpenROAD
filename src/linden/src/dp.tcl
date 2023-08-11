sta::define_cmd_args "linden_detailed_placement" {
    [--timing-data timing.pb]
    [--filler-list filler-masters]
    [--not-to-stop-when-illegal]
}

proc linden_detailed_placement { args } {
    linden::StrDeque a
    foreach x $args {
        a push_back $x
    }
    linden::lindendp_gogogo a
}

sta::define_cmd_args "linden_incremental_legalize" {
    --high-priority-cells cell-names
    [--filler-list filler-masters]
    [--not-to-stop-when-illegal]
}

proc linden_incremental_legalize { args } {
    linden::StrDeque a
    foreach x $args {
        a push_back $x
    }
    linden::linden_incr_legalize a
}

