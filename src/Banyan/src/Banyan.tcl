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
  banyan::clearWires $ex_signal $ex_clock
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

# This method no longer guarantees that the input def will only have wires changes compared to the existing def in OpenDB.
# The caller needs to make the above assumptions.
sta::define_cmd_args "banyan_load_def" { deffile }
proc banyan_load_def { deffile } {
  banyan::loadDef $deffile
}

sta::define_cmd_args "banyan_optimize_congestion" { max_layer }
proc banyan_optimize_congestion { max_layer } {
  banyan::optimizeCongestion $max_layer
}

sta::define_cmd_args "banyan_incremental_route_begin" { [-max_layer layer] [-work_dir out_path] [-init_db db] }
proc banyan_incremental_route_begin { args } {
  sta::parse_key_args "banyan_incremental_route_begin" args keys {-max_layer -work_dir -init_db} flags {}
  set out_path "/tmp/incr_route/"
  if { [info exists keys(-work_dir)] } {
    set out_path $keys(-work_dir)
  }
  file mkdir $out_path
  set layer ""
  if { [info exists keys(-max_layer)] } {
    set layer $keys(-max_layer)
  } else {
    utl::error BY 1000 "must give a max_layer"
  }
  set db ""
  if { [info exists keys(-init_db)] } {
    set db $keys(-init_db)
  }

  banyan::newIncrementalRoute $layer $out_path $db
}

proc banyan_incremental_route {} {
  return [banyan::incrementalRoute]
}

proc banyan_incremental_route_end {} {
  banyan::deleteIncrementalRoute
}