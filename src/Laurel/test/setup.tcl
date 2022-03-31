read_liberty sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib
read_lef sky130hd/sky130hd.tlef
read_lef sky130hd/sky130hd_std_cell.lef

read_verilog verilog/setup.v

link_design top

read_sdc sdc/function1.sdc

puts "end reading sdc\n"

estimate_parasitics -placement

puts "-----------------\nreport func1 b4repair \n-----------------"
report_checks

laurel -setup

puts "end repair timing"

puts "-----------------\nreport func2 after repair \n-----------------"
report_checks

write_verilog results/setup.result.v

