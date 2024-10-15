source "helpers.tcl"
set ::env(BANYAN_SUPPRESS_EXE_LOG) true
read_lef Nangate45.lef
read_def gcd_replace.def 

linden_routability_driven_legalize \
    --routing-top-layer metal5 \
    --suppress-output

set def_file [make_result_file rddp.def]
write_def $def_file
diff_file rddp.defok $def_file
