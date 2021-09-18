sta::define_cmd_args "banyan_route" {
    [-log_level level]
    [-debug_output prefix]
    [-debug_save_txt]
    [-max_via_stack]
}


proc banyan_route { args } {
  sta::parse_key_args "banyan_route" args \
      keys {-log_level -debug_output -max_via_stack} \
      flags {-debug_save_txt}

  sta::check_argc_eq0 "banyan_route" $args

  if { [info exists keys(-log_level)] } {
    set log_level $keys(-log_level)
  } else {
    set log_level "INFO"
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

  banyan::banyan_route $log_level $debug_output $debug_save_txt $max_via_stack
}
