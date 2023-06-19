sta::define_cmd_args "analyze_banyan_cmap_file" {\
    [-cmap_pb_file input_cmap_pb_file]\
    [-report_json_file output_report_json_file]\
}

proc analyze_banyan_cmap_file { args } {
  sta::parse_key_args "analyze_banyan_cmap_file" args \
  keys {\
    -cmap_pb_file \
    -report_json_file \
  }

  set cmap_pb_file ""
  set report_json_file ""
  if { [info exists keys(-cmap_pb_file)] } { 
    set cmap_pb_file $keys(-cmap_pb_file)
  }
  if { [info exists keys(-report_json_file)] } {
    set report_json_file $keys(-report_json_file)
  }
  cgr::analyze_banyan_cmap_file_cmd $cmap_pb_file $report_json_file
}