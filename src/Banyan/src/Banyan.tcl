proc banyan { args } {
  utl::info BY 6 "run banyan with args: '$args'"
  banyan::banyan $args true
}

sta::define_cmd_args "banyan_pa_eval" {[#<extra args forwarded to `eval banyan pa_eval`>]}
proc banyan_pa_eval { args } {
    banyan::banyan [linsert $args 0 "pa_eval"] false
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

sta::define_cmd_args "regression_RC" { spef setRC_tcl [-plot file_prefix] [-corner corner_name] }

proc regression_RC { spef setRC_tcl args } {
  sta::parse_key_args "regression_RC" args keys {-plot -corner} flags {}
  set plot ""
  if { [info exists keys(-plot)] } {
    set plot $keys(-plot)
  }

  set corner ""
  if { [info exists keys(-corner)] } {
    set corner $keys(-corner)
  }

  banyan::regression_RC_cmd $spef $setRC_tcl $plot $corner
}

sta::define_cmd_args "banyan_save_db" { dbfile }
proc banyan_save_db { dbfile } {
  banyan::saveDB $dbfile
}

sta::define_cmd_args "banyan_load_db" { dbfile }
proc banyan_load_db { dbfile } {
  banyan::loadDB $dbfile
}

sta::define_cmd_args "banyan_load_def" { deffile }
proc banyan_load_def { deffile } {
  banyan::loadDef $deffile
}

sta::define_cmd_args "banyan_diff_defs" { deffileA deffileB }
proc banyan_diff_defs { deffileA deffileB } {
  banyan::diffDefs $deffileA $deffileB
}