sta::define_cmd_args "linden_global_placement" {\
    [-uniform_density] \
    [-zigzag true|false] \
    [-timing_driven] \
    [-density target_density]\
    [-overflow overflow]\
    [-pad_left pad_left]\
    [-pad_right pad_right]\
    [-initial_place_policy initial_place_policy]\
}

sta::define_cmd_args "linden_macro_placement" {\
    [-uniform_density] \
    [-density target_density]\
    [-overflow overflow]\
}

proc linden_global_placement { args } {
  sta::parse_key_args "linden_global_placement" args \
    keys {\
      -initial_place_policy \
      -density \
      -overflow \
      -zigzag \
      -pad_left -pad_right} \
    flags { \
      -uniform_density \
      -timing_driven \
    }

  # zigzag settings
  set zigzag false
  if { [info exists keys(-zigzag)] } {
    set zigzag $keys(-zigzag)
  }
  gpl::set_linden_zigzag_cmd $zigzag

  # density settings
  set target_density 1.0
  set uniform_mode [info exists flags(-uniform_density)]
  gpl::set_linden_use_uniform_density_cmd $uniform_mode

  if { [info exists keys(-density)] } {
    set target_density $keys(-density)
    sta::check_positive_float "-density" $target_density
  }
  gpl::set_linden_density_cmd $target_density

  set overflow 0.1
  if { [info exists keys(-overflow)] } {
    set overflow $keys(-overflow)
    sta::check_positive_float "-overflow" $overflow
  }
  gpl::set_linden_overflow_cmd $overflow

  set pad_left 0
  set pad_right 0
  if { [info exists keys(-pad_left)] } {
    set pad_left $keys(-pad_left)
  }
  if { [info exists keys(-pad_right)] } {
    set pad_right $keys(-pad_right)
  }
  sta::check_positive_integer "-pad_left" $pad_left
  sta::check_positive_integer "-pad_right" $pad_right
  gpl::set_linden_pad_left_cmd $pad_left
  gpl::set_linden_pad_right_cmd $pad_right

  set timing_driven [info exists flags(-timing_driven)]
  gpl::set_linden_timing_driven_mode $timing_driven
  if { $timing_driven } {
    if { [get_libs -quiet "*"] == {} } {
      utl::error GPL 1121 "No liberty libraries found."
    }
  }

  if { [info exists keys(-initial_place_policy)] } {
    set initial_place_policy $keys(-initial_place_policy)
    gpl::set_linden_initial_place_policy_cmd $initial_place_policy
  }

  if { [ord::db_has_rows] } {
    sta::check_argc_eq0 "linden_global_placement" $args
    gpl::lindengp_run_cmd
  } else {
    utl::error GPL 1130 "No rows defined in design. Use initialize_floorplan to add rows."
  }
}

proc linden_macro_placement { args } {
  sta::parse_key_args "linden_macro_placement" args \
    keys {\
      -density \
      -overflow } \
    flags { \
      -uniform_density \
    }

  # density settings
  set target_density 1.0
  set uniform_mode [info exists flags(-uniform_density)]
  gpl::set_lindenmp_use_uniform_density_cmd $uniform_mode

  if { [info exists keys(-density)] } {
    set target_density $keys(-density)
    sta::check_positive_float "-density" $target_density
  }
  gpl::set_lindenmp_density_cmd $target_density

  set overflow 0.1
  if { [info exists keys(-overflow)] } {
    set overflow $keys(-overflow)
    sta::check_positive_float "-overflow" $overflow
  }
  gpl::set_lindenmp_overflow_cmd $overflow

  gpl::lindenmp_run_cmd
}

namespace eval gpl {
proc linden_global_placement_debug { args } {
  sta::parse_key_args "linden_global_placement_debug" args \
      keys {-pause -update} \
      flags {-draw_bins -initial}

  set pause 10
  if { [info exists keys(-pause)] } {
    set pause $keys(-pause)
    sta::check_positive_integer "-pause" $pause
  }

  set update 10
  if { [info exists keys(-update)] } {
    set update $keys(-update)
    sta::check_positive_integer "-update" $update
  }
  set initial [info exists flags(-initial)]
  gpl::set_linden_debug_cmd $pause $update $initial
}

proc linden_macro_placement_debug { args } {
  sta::parse_key_args "linden_macro_placement_debug" args \
      keys {-pause -update}

  set pause 10
  if { [info exists keys(-pause)] } {
    set pause $keys(-pause)
    sta::check_positive_integer "-pause" $pause
  }

  set update 10
  if { [info exists keys(-update)] } {
    set update $keys(-update)
    sta::check_positive_integer "-update" $update
  }
  gpl::set_lindenmp_debug $pause $update
}

}

sta::define_cmd_args "linden_remove_fillers" {
}

proc linden_remove_fillers {} {
  gpl::linden_remove_fillers_cmd
}