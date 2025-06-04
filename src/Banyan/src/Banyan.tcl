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

sta::define_cmd_args "banyan_optimize_congestion" { min_layer max_layer }
proc banyan_optimize_congestion { min_layer max_layer } {
  banyan::optimizeCongestion $max_layer
  write_def /tmp/after_rddp.def
  linden_incremental_legalize --min-layer $min_layer --max-layer $max_layer --baseline /tmp/after_rddp.def
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

proc banyan_add_placement_blockage { args } {
  banyan::add_placement_blockage $args
}

sta::define_cmd_args "banyan_repair_antennas" { diode_cell \
                                         [-min_layer min_layer] \
                                         [-max_layer max_layer] \
                                         [-max_iter max_iter] \
                                         [-max_fanout max_fanout]}

proc banyan_repair_antennas { args } {
  sta::parse_key_args "banyan_repair_antennas" args \
    keys {-history -min_layer -max_layer -max_iter} flags {}
  
  if {[llength [info commands lrl::register_sc_editer]] > 0} {
    lrl::register_sc_editer
  } else {
    puts "\[Info\] laurel editer not found, use default editer."
  }
  
  set diode_mterm "NULL"
  if { [llength $args] == 0 } {
      # repairAntennas locates diode
      set diode_mterm "NULL"
  } elseif { [llength $args] == 1 } {
    set db [ord::get_db]
    set diode_cell [lindex $args 0]

    set diode_master [$db findMaster $diode_cell]
    if { $diode_master == "NULL" } {
      utl::error BY 69 "Diode cell $diode_cell not found."
    }

    set diode_mterms [$diode_master getMTerms]
    set non_pg_count 0
    foreach mterm $diode_mterms {
      if { [$mterm getSigType] != "POWER" && [$mterm getSigType] != "GROUND" } {
        set diode_mterm $mterm
        incr non_pg_count
      }
    }

    if { $non_pg_count > 1 } {
      utl::error BY 73 "Diode cell has more than one non power/ground port."
    }
  } else {
    utl::error BY 245 "Too arguments to banyan_repair_antennas."
  }
  set min_layer $keys(-min_layer)
  set max_layer $keys(-max_layer)
  set max_iter 1
  if { [info exists keys(-max_iter)] } {
    set max_iter $keys(-max_iter)
    sta::check_positive_integer "-max_iter" $max_iter
  }
  set max_fanout 150
  if { [info exists keys(-max_fanout)] } {
    set max_fanout $keys(-max_fanout)
    sta::check_positive_integer "-max_fanout" $max_fanout
  }
  return [banyan::repair_antennas $diode_mterm $min_layer $max_layer $max_iter $max_fanout]
}