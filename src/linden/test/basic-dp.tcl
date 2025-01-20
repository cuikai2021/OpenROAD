source "helpers.tcl"
read_lef Nangate45.lef
read_def gcd_replace.def

linden_detailed_placement

set def_file [make_result_file basic-dp.def]
write_def $def_file
check_placement
diff_file basic-dp.defok $def_file
