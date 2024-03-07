sta::define_cmd_args "linden_macro_placement" {\
    [-uniform_density] \
    [-density target_density]\
    [-overflow overflow]\
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
  linden::set_lindenmp_use_uniform_density_cmd $uniform_mode

  if { [info exists keys(-density)] } {
    set target_density $keys(-density)
    sta::check_positive_float "-density" $target_density
  }
  linden::set_lindenmp_density_cmd $target_density

  set overflow 0.1
  if { [info exists keys(-overflow)] } {
    set overflow $keys(-overflow)
    sta::check_positive_float "-overflow" $overflow
  }
  linden::set_lindenmp_overflow_cmd $overflow

  linden::lindenmp_run_cmd
}

namespace eval linden {

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
  linden::set_lindenmp_debug $pause $update
}

}

