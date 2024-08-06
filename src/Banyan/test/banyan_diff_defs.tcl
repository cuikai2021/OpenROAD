source "helpers.tcl"
source Nangate45/Nangate45.vars

suppress_message BY 1
read_lef $tech_lef
read_lef $std_cell_lef

banyan_diff_defs nangate_test.def nangate_test_reorder_wires.def

banyan_diff_defs nangate_test.def nangate_test_remove_wires.def
