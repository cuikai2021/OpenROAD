# ICBench Methods

sta::define_cmd_args "laurel_fix_hold" {[-clk] [-uncertainty uncertainty]\
                                        [-slack_margin slack_margin]}

proc laurel_fix_hold { args } {
  sta::parse_key_args "fix_hold" args \
    keys {-uncertainty -slack_margin} \
    flags {-clk}
  
  if { [info exists keys(-uncertainty)] } {
    lrl::set_uncertainty $keys(-uncertainty)
  }
  if { [info exists keys(-slack_margin)] } {
    lrl::set_slack_margin $keys(-slack_margin)
  }
  if { [info exists flags(-clk)] } {
    lrl::analyze_clk
  }
  lrl::fix_hold_cmd
}

sta::define_cmd_args "laurel_fix_setup" {[-slack_margin slack_margin]}

proc laurel_fix_setup { args } {
  sta::parse_key_args "fix_setup" args \
    keys {-slack_margin} \
    flags {}
  
  if { [info exists keys(-slack_margin)] } {
    lrl::set_slack_margin $keys(-slack_margin)
  }
  lrl::fix_setup_cmd
}

sta::define_cmd_args "laurel" {[-slack_margin slack_margin] \
                                [-spef spef] [-iter iter] \
                                [-fast] [-setup] [-hold] [-wire_budgeting] [-ccopt]\
                                [-debug_report] \
                                [-rc] [-iterative] [-multi_mode] [-global]}

proc laurel { args } {
  sta::parse_key_args "laurel" args \
    keys {-slack_margin -spef -iter} \
    flags {-fast -setup -hold -ccopt \
            -wire_budgeting -debug_report \
            -rc -iterative -multi_mode -global}
  
  if { [info exists keys(-slack_margin)] } {
    lrl::set_slack_margin $keys(-slack_margin)
  }

  if { [info exists flags(-debug_report)] } {
    lrl::debug_report 
  }
  
  if { [info exists flags(-rc)] } {
    lrl::set_wire_bud_mode_rc
  }

  if { [info exists keys(-spef)] } {
    lrl::set_spef_file $keys(-spef)
  }
  set rsz_cnt 0
  set buf_cnt 0
  set iter_cnt 1
  set multi_mode "false"
  set partial "true"
  if { [info exists flags(-multi_mode)] } {
    set multi_mode "true"
  }
  if { [info exists flags(-global)] } {
    set partial "false"
  }
  if { [info exists flags(-iterative)] } {
    set iter_cnt 5
    set partial "true"
  }
  if { [info exists keys(-iter)] } {
    set iter_cnt $keys(-iter)
  }
  if { [info exists flags(-fast)] } {
    set rsz_cnt [lrl::fast_opt_cmd $iter_cnt]
  } else {
    if { [info exists flags(-ccopt)] } {
      set rsz_cnt [lrl::ccopt_cmd]
    } elseif { [info exists flags(-setup)] } {
      set rsz_cnt [lrl::fix_setup_cmd $iter_cnt]
    }
    if { [info exists flags(-hold)] } {
      set buf_cnt [lrl::fix_hold_cmd $multi_mode $partial]
    }
    if { [info exists flags(-wire_budgeting)] } {
      lrl::wire_budgeting_cmd
    }
  }
  return [expr $rsz_cnt + $buf_cnt]
}

sta::define_cmd_args "report_violations" {[-max] [-min] [-ignore_top_ports]}

proc report_violations {args} {
  sta::parse_key_args "report_violations" args \
    keys {} \
    flags {-min -max -ignore_top_ports}
  
  set ignore_top_ports "false"
  set min_max "max"
  if { [info exists flags(-min)] } {
    set min_max "min"
  }
  if { [info exists flags(-ignore_top_ports)] } {
    set ignore_top_ports "true"
  }


  lrl::report_violations_cmd $min_max $ignore_top_ports
}
