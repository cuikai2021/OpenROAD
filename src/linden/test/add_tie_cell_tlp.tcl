source "helpers.tcl"

# Load Design
read_lef Nangate45.lef
read_liberty Nangate45_typ.lib
read_def add_tie_cell_tlp.def

# Add Tie Cell
linden_add_tiecell --tie-cell LOGIC0_X1 Z LOGIC1_X1 Z --max-fanout 10

# Check
set def_res [make_result_file add_tie_cell_tlp.res.def]
write_def $def_res
diff_file add_tie_cell_tlp.defok $def_res
