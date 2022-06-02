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

sta::define_cmd_args "laurel" {[-slack_margin slack_margin]\
                                [-spef spef] \
                                [-setup] [-hold] [-wire_budgeting]}

proc laurel { args } {
  sta::parse_key_args "laurel" args \
    keys {-slack_margin -spef} \
    flags {-setup -hold -wire_budgeting}
  
  if { [info exists keys(-slack_margin)] } {
    lrl::set_slack_margin $keys(-slack_margin)
  }

  if { [info exists keys(-spef)] } {
    lrl::set_spef_file $keys(-spef)
  }

  if { [info exists flags(-setup)] } {
    lrl::fix_setup_cmd
  }
  if { [info exists flags(-hold)] } {
    lrl::fix_hold_cmd
  }
  if { [info exists flags(-wire_budgeting)] } {
    lrl::wire_budgeting_cmd
  }
}