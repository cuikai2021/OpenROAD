source "helpers.tcl"
source "resizer_helpers.tcl"

read_liberty Nangate45/Nangate45_typ.lib
read_lef Nangate45/Nangate45.lef
read_def remove_tie_cell_tiecell_with_nontieport.def

remove_tie_cell -tie_low_cell AND2_X1 \
                -tie_low_pin       ZN \
                -tie_high_cell OR2_X1 \
                -tie_high_pin      ZN

set def_file [make_result_file remove_tie_cell_tiecell_with_nontieport.def]
write_def $def_file
diff_files remove_tie_cell_tiecell_with_nontieport.defok $def_file

puts "check_ties LOGIC0_X1:"
check_ties LOGIC0_X1
puts "report_ties LOGIC0_X1:"
report_ties LOGIC0_X1
puts "check_ties LOGIC1_X1:"
check_ties LOGIC1_X1
puts "report_ties LOGIC1_X1:"
report_ties LOGIC1_X1
puts "check_in_core:"
check_in_core
