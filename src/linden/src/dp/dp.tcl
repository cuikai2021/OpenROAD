sta::define_cmd_args "linden_detailed_placement" {
    [--timing-data timing.pb]
    [--filler-list filler-masters]
    [--not-to-stop-when-illegal]
    [--disable-updating-parasitics-after-legalizing]
}

proc linden_detailed_placement { args } {
    linden::StrDeque a
    foreach x $args {
        a push_back $x
    }
    linden::lindendp_gogogo a
}

sta::define_cmd_args "linden_incremental_legalize" {
    [--critical-cells cell-names]
    [--baseline baseline.def]
    [--timing-data timing.pb]
    [--filler-list filler-masters]
    [--not-to-stop-when-illegal]
    [--disable-updating-parasitics-after-legalizing]
}

proc linden_incremental_legalize { args } {
    linden::StrDeque a
    foreach x $args {
        a push_back $x
    }
    linden::linden_incr_legalize a
}

proc linden_routability_driven_legalize { args } {
    linden::StrDeque a
    foreach x $args {
        a push_back $x
    }
    linden::routability_driven_legalize a
}

sta::define_cmd_args "linden_optimize" {
    [--timing-data timing.pb]
}

proc linden_optimize { args } {
    linden::StrDeque a
    foreach x $args {
        a push_back $x
    }
    linden::lindenopt_gogogo a
}


namespace eval linden::dp {

# `default-config` accepts the following args:
# * `--filler-list FILLERS`
#   A list of filler-cell names, separated by white spaces.
# * `--not-to-stop-when-illegal`
#   When the design is failed to do legalization, linden DP will not terminate.
# * `--suppress-output`
#   Suppresses output of linden DP.
# * `--routing-top-layer`
#   Specifies the top layer for routability-driven DP.
# * `--extra-pg-spacing LAYER SPACING`
#   Specifies extra SPACING for P/G metal on LAYER.
#   SPACING is in nm.
#
# ps.
# `sta::define_cmd_args` does not work for commands under namespaces.


proc default-config { args } {
    linden::StrDeque a
    foreach x $args {
        a push_back $x
    }
    linden::dp_default_config_gogogo a
}

# end namespace linden::dp
}
