source helpers.tcl
set test_name simple01-rd
read_liberty ./Nangate45_typ.lib
read_lef ./Nangate45.lef
read_def ./$test_name.def

set ::env(BANYAN_SUPPRESS_EXE_LOG) true
linden::set_highest_layer_to_route metal10
linden_global_placement \
    -routability_driven \
    -overflow_to_estimate_cmap 0.69

set def_file [make_result_file $test_name.def]
write_def $def_file
diff_file $def_file $test_name.defok
