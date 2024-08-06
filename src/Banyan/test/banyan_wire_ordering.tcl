
source "helpers.tcl"
source Nangate45/Nangate45.vars

suppress_message BY 1
read_lef $tech_lef
read_lef $std_cell_lef


read_def nangate_test.def

banyan_load_def nangate_test_wire_unordered.def

set outputfile [make_result_file nangate_test_wire_ordered.def]

write_def $outputfile

diff_files nangate_test_wire_ordered.defok $outputfile