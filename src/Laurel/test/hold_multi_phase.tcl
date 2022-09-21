read_liberty Nangate45/Nangate45_fast.lib
read_lef Nangate45/Nangate45_tech.lef
read_lef Nangate45/Nangate45_stdcell.lef

read_verilog verilog/hold_multi_phase.v

link_design top

read_sdc sdc/func1.sdc


estimate_parasitics -placement

report_worst_slack -max
puts "tns max"
puts [sta::total_negative_slack -max]
report_worst_slack -min
puts "tns min"
puts [sta::total_negative_slack -min]
report_design_area
# set_debug_level LRL "LRL" 4
puts [laurel -hold]

report_worst_slack -max
puts "tns max"
puts [sta::total_negative_slack -max]
report_worst_slack -min
puts "tns min"
puts [sta::total_negative_slack -min]
report_design_area

write_verilog results/hold_multi_phase.v

