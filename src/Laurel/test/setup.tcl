read_liberty sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib
read_lef sky130hd/sky130hd.tlef
read_lef sky130hd/sky130hd_std_cell.lef

read_verilog verilog/setup.v

link_design top

read_sdc sdc/function1.sdc

puts "end reading sdc\n"

estimate_parasitics -placement

report_worst_slack -max
report_design_area
# set_debug_level LRL "LRL" 4
puts [laurel -setup]
puts "end repair timing"
report_worst_slack -max
report_design_area


write_verilog results/setup.result.v

