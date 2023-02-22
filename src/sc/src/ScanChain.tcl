sta::define_cmd_args "scan_chain_reorder" {[]}

proc scan_chain_reorder { args } {
  sta::parse_key_args "scan_chain_reorder" args \
    keys {} \
    flags {}

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