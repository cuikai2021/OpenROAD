source "helpers.tcl"
source "resizer_helpers.tcl"

read_liberty Nangate45/Nangate45_typ.lib
read_lef Nangate45/Nangate45.lef
read_lef pad.lef
read_def remove_tie_cell_constrained_common.def

set_max_delay -through [get_nets tie_net_3] 10
set_max_delay -through [get_nets tie_net_7] 10

remove_tie_cell -tie_low_cell  LOGIC0_X1 \
                -tie_low_pin   Z         \
                -tie_high_cell LOGIC1_X1 \
                -tie_high_pin  Z

set def_file [make_result_file remove_tie_cell_constrained_net.def]
write_def $def_file
diff_files remove_tie_cell_constrained_net.defok $def_file

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
