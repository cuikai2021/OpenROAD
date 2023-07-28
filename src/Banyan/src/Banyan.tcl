
proc banyan { args } {
  banyan::banyan $args
}

proc banyan_run_gui {} {
  puts "banyan_run_gui"
  banyan::banyanRunGui
}

proc banyan_drc_patch {} {
  banyan::drcPatch
}

proc banyan_merge_wire_segment { } {
  banyan::mergeWireSegment
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

sta::define_cmd_args "regression_RC" { spef setRC_tcl }

proc regression_RC { spef setRC_tcl } {
  banyan::regression_RC_cmd $spef $setRC_tcl
}
