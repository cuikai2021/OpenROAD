proc banyan { args } {
  set now [clock format [clock seconds] -format %y%m%d-%T-%Z]
  utl::info BY 6 "run banyan with args: `$args' at $now"
  banyan::banyan $args
}

sta::define_cmd_args "banyan_clear_wires" {
  [-exclude_signal] \
  [-exclude_clock] \
}

proc banyan_clear_wires { args } {
  sta::parse_key_args "check_abanyan_clear_wiresntennas" args \
    keys {} \
    flags { -exclude_signal -exclude_clock }

  set ex_signal [info exists flags(-exclude_signal)]
  set ex_clock [info exists flags(-exclude_clock)]
  banyan::banyanClearWires $ex_signal $ex_clock
}

sta::define_cmd_args "regression_RC" { spef setRC_tcl [-plot file_prefix] }

proc regression_RC { spef setRC_tcl args } {
  sta::parse_key_args "regression_RC" args keys {-plot} flags {}
  set plot ""
  if { [info exists keys(-plot)] } {
    set plot $keys(-plot)
  }

  banyan::regression_RC_cmd $spef $setRC_tcl $plot
}

sta::define_cmd_args "banyan_save_db" { dbfile }
proc banyan_save_db { dbfile } {
  banyan::saveDB $dbfile
}

sta::define_cmd_args "banyan_load_db" { dbfile }
proc banyan_load_db { dbfile } {
  banyan::loadDB $dbfile
}
