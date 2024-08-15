# -continue
# GP starts to spread the current design with the pre-dumped internal states.
# So, `cgr::set_cgr_params_file_cmd` is required.
# And it usually works with `-initial_place_policy def` together.
sta::define_cmd_args "linden_global_placement" {\
    [-uniform_density] \
    [-density target_density]\
    [-overflow overflow]\
    [-initial_place_policy initial_place_policy]\
    [-continue] \
    [-timing_driven] \
    [-routability_driven]\
    [-overflow_to_estimate_cmap overflow] \
}

proc linden_global_placement { args } {
    linden::StrDeque a
    foreach x $args {
        a push_back $x
    }
    linden::lindengp_run_cmd a
}

sta::define_cmd_args "linden_random_global_placement" {\
}

# Generates a random but stable (global) placement.
proc linden_random_global_placement {} {
    linden::lindengp_random_run_cmd
}
