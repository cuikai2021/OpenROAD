sta::define_cmd_args "linden_lpbased_postdrv_itdp" {\
  [-timing_data_file timing_data_file]\
}

proc linden_lpbased_postdrv_itdp { args } {
  sta::parse_key_args "linden_lpbased_postdrv_itdp" args \
    keys { \
      -timing_data_file \
    }
  linden::linden_lpbased_postdrv_itdp $keys(-timing_data_file)
}

