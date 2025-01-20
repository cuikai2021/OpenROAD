source "helpers.tcl"
read_lef Nangate45.lef
read_def gcd_replace.def

linden_incremental_legalize --baseline gcd.baseline.def

set def_file [make_result_file incr-dp.baseline.def]
write_def $def_file
check_placement
diff_file incr-dp.baseline.defok $def_file
