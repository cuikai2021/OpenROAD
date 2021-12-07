sta::define_cmd_args "banyan_route" {
    [-log_level level]
    [-debug_output prefix]
    [-debug_save_txt]
    [-enable_route default true]
    [-enable_maze_top default true]
    [-max_via_stack]
    [-min_layer]
    [-max_layer]
    [-cts]
}


proc banyan_route { args } {
  sta::parse_key_args "banyan_route" args \
      keys {-log_level -debug_output -max_via_stack -min_layer -max_layer -enable_route -enable_maze_top} \
      flags {-debug_save_txt -cts}

  sta::check_argc_eq0 "banyan_route" $args

  if { [info exists keys(-log_level)] } {
    set log_level $keys(-log_level)
  } else {
    set log_level "DEBUG"
  }

  if { [info exists keys(-debug_output)] } {
    set debug_output $keys(-debug_output)
  } else {
    set debug_output ""
  }

  if { [info exists keys(-max_via_stack)] } {
    sta::check_positive_integer "-max_via_stack" $keys(-max_via_stack)
    set max_via_stack $keys(-max_via_stack)
  } else {
    set max_via_stack ""
  }

  set debug_save_txt [info exists flags(-debug_save_txt)]  

  if { [info exists keys(-min_layer)] } {
    set min_layer $keys(-min_layer)
  } else {
    set min_layer "M2"
  }

  if { [info exists keys(-max_layer)] } {
    set max_layer $keys(-max_layer)
  } else {
    set max_layer ""
  }

  set cts [info exists flags(-cts)]  

  if { [info exists keys(-enable_route)] } {
    check_bool "-enable_route" $keys(-enable_route)
    set enable_route $keys(-enable_route)
  } else {
    set enable_route "true"
  }

  if { [info exists keys(-enable_maze_top)] } {
    check_bool "-enable_maze_top" $keys(-enable_maze_top)
    set enable_maze_top $keys(-enable_maze_top)
  } else {
    set enable_maze_top "true"
  }
  
  banyan::banyan_route $log_level $debug_output $debug_save_txt $max_via_stack $min_layer $max_layer $cts $enable_route $enable_maze_top
}


proc check_bool { cmd_arg arg } {
  if {!([string equal $arg "true"] || [string equal $arg "false"])} {
    sta_error 418 "$cmd_arg '$arg' is not true and false."
  }
}