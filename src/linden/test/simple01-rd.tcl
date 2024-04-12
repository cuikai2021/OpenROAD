source helpers.tcl
set test_name simple01-rd

read_lef ./nangate45.lef
read_def ./$test_name.def

set ::env(BANYAN_SUPPRESS_EXE_LOG) true
set_congestion_repair_mode -routing_max_layer metal10
linden_global_placement -routability_driven

set def_file [make_result_file $test_name.def]
write_def $def_file
diff_file $def_file $test_name.defok
