source "helpers.tcl"
read_lef for_pin_access.lef
read_def extra-pg-spacing.def

set ::env(LINDEN_DP_ALGORITHM) klotski
linden::dp::default-config --extra-pg-spacing metal2 840
linden_detailed_placement

set def_file [make_result_file extra-pg-spacing.def]
write_def $def_file
diff_file extra-pg-spacing.defok $def_file
