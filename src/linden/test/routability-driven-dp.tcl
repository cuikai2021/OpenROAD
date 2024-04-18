source "helpers.tcl"
read_lef Nangate45.lef
read_def gcd_replace.def 

set ::env(BANYAN_SUPPRESS_EXE_LOG) true
linden_routability_driven_detailed_placement \
    --routing_max_layer metal5 \
    --suppress-output
set def_file [make_result_file routability-driven-dp.baseline.def]
write_def $def_file
diff_file routability-driven-dp.defok $def_file 