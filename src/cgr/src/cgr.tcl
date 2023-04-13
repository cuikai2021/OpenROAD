sta::define_cmd_args "congestion_repair" {[]}

sta::define_cmd_args "set_congestion_repair_mode" {\
    [-params_file cgr_params_file_path]\
    [-routing_max_layer max_layer]\
}

proc congestion_repair { args } {
  sta::parse_key_args "congestion_repair" args \
    keys {} \
    flags {}

  for {set iter 1} {$iter <= 10} {set iter [expr $iter+1]} {
    set res [cgr::run]
    if {$res == 0} {
      break
    }
    linden_detailed_placement
  }
  cgr::done
}

proc set_congestion_repair_mode { args } {
  sta::parse_key_args "set_congestion_repair_mode" args \
    keys {\
      -params_file \
      -routing_max_layer \
    } \
    flags { \
    }

  if { [info exists keys(-params_file)] } { 
    set params_file $keys(-params_file)
    cgr::set_cgr_params_file_cmd $params_file
  }

  if { [info exists keys(-routing_max_layer)] } { 
    set routing_max_layer $keys(-routing_max_layer)
    cgr::set_routing_max_layer_cmd $routing_max_layer
  }
}