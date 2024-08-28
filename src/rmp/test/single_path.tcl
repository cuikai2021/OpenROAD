source "./helpers.tcl"
read_lef tcbn65lp_std.lef
set liberty_library  tcbn65lpwc.modi.lib
read_liberty $liberty_library

read_verilog single_path.v
link_design top


set output_pin [get_pins "inv8/ZN"]
set tiehi "LOGIC1_X1/Z"
set tielo "LOGIC0_X1/Z"

restructure -liberty_file $liberty_library -target area -abc_logfile results/noise.log -tielo_port $tielo -tiehi_port $tiehi -work_dir ./results -cutremap true -so_cut true -head_pin $output_pin

set verilog_file [make_result_file single_path_op.v]
write_verilog $verilog_file
report_file $verilog_file



