sta::define_cmd_args "scan_chain_reorder" {[]}

proc scan_chain_reorder { args } {
  sta::parse_key_args "scan_chain_reorder" args \
    keys {} \
    flags {}
  # Laurel is "later" than sc, so bind this editer when running.
  if {[llength [info commands lrl::register_sc_editer]] > 0} {
    lrl::register_sc_editer
  } else {
    puts "\[Info\] laurel editer not found, use default editer."
  }
  sc::reorder_cmd
}

sta::define_cmd_args "scan_chain_trace" {[]}

proc scan_chain_trace { args } {
  sta::parse_key_args "scan_chain_trace" args \
    keys {} \
    flags {}

  sc::trace_cmd
}

sta::define_cmd_args "report_scan_chain" {filename}

proc report_scan_chain { filename } {
  sc::report_scan_chain [file nativename $filename]
}