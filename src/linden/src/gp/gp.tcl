# -continue
# GP starts to spread the current design with the pre-dumped internal states.
# So, `cgr::set_cgr_params_file_cmd` is required.
# And it usually works with `-initial_place_policy def` together.
sta::define_cmd_args "linden_global_placement" {\
    [-uniform_density] \
    [-timing_driven] \
    [-routability_driven]\
    [-density target_density]\
    [-overflow overflow]\
    [-pad_left pad_left]\
    [-pad_right pad_right]\
    [-initial_place_policy initial_place_policy]\
    [-focus_insts focus_insts_file]\
    [-focus_nets focus_nets_file]\
    [-continue] \
}

proc linden_global_placement { args } {
    linden::StrDeque a
    foreach x $args {
        a push_back $x
    }
    linden::lindengp_run_cmd a

    # if { [info exists keys(-pad_left)] } {
    #     set pad_left $keys(-pad_left)
    #     sta::check_positive_integer "-pad_left" $pad_left
    #     linden::set_linden_pad_left_cmd $pad_left
    # }
    # if { [info exists keys(-pad_right)] } {
    #     set pad_right $keys(-pad_right)
    #     sta::check_positive_integer "-pad_right" $pad_right
    #     linden::set_linden_pad_right_cmd $pad_right
    # }

#   set routability_driven [info exists flags(-routability_driven)]
#   linden::set_linden_routability_driven_mode $routability_driven

#   if { [info exists keys(-focus_insts)] } {
#     set focus_insts $keys(-focus_insts)
#     linden::set_linden_focus_insts_cmd $focus_insts
#   }

#   if { [info exists keys(-focus_nets)] } {
#     set focus_nets $keys(-focus_nets)
#     linden::set_linden_focus_nets_cmd $focus_nets
#   }

#   if { [info exists flags(-continue)] } {
#     linden::enable_linden_continuation
#   }
}

