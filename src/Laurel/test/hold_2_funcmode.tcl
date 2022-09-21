read_liberty Nangate45/Nangate45_fast.lib
read_lef Nangate45/Nangate45_tech.lef
read_lef Nangate45/Nangate45_stdcell.lef

read_verilog verilog/hold_tri_2_funcmode.v

link_design top

read_sdc -mode scan sdc/scan.sdc
read_sdc -mode func1 sdc/func1.sdc
read_sdc -mode func2 sdc/func2.sdc


estimate_parasitics -placement

puts "-----------------\nreport scan b4repair \n-----------------"
report_checks -sdc scan -path_delay min
report_worst_slack -max

puts "-----------------\nreport func1 b4repair \n-----------------"
report_checks -sdc func1 -path_delay min
report_worst_slack -max

puts "-----------------\nreport func2 b4repair \n-----------------"
report_checks -sdc func2 -path_delay min
report_worst_slack -max
# set_debug_level LRL "LRL" 4
puts [laurel -hold -global -multi_mode]

puts "-----------------\nreport scan after repair \n-----------------"
report_checks -sdc scan -path_delay min
report_worst_slack -max

puts "-----------------\nreport func1 after repair \n-----------------"
report_checks -sdc func1 -path_delay min
report_worst_slack -max

puts "-----------------\nreport func2 after repair \n-----------------"
report_checks -sdc func2 -path_delay min
report_worst_slack -max

write_verilog results/hold.result.v

