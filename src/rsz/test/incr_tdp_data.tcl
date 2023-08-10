read_liberty sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib
read_lef sky130hd/sky130hd.tlef
read_lef sky130hd/sky130hd_std_cell.lef

read_verilog verilog/incr_tdf_data.v

link_design top

read_sdc sdc/incr_tdf_data.sdc

set_wire_rc -resistance 0.0001 -capacitance 0.00001

report_worst_slack -max
report_design_area


set pb_file "results/incr_tdp_data.pb"

rsz::write_tdp_data $pb_file

puts "\n-------test the dumped result-------"
rsz::read_tdp_data $pb_file

