source "./helpers.tcl"
set test_path "../../rmp/test/cases/restr_unit/rm"
set liberty_library  $test_path/libs/ss/ua11lscef15bdrll_135c125_ss.lib
read_liberty $liberty_library
read_lef $test_path/lefs/ua11lscef15bdrll_6m2t.lef
read_lef $test_path/lefs/ua11lscef15bdrll.lef

read_verilog multilevel.v
link_design multilevel


set output_pin [get_pins "nr1/Z"]
set tiehi "LOGIC1_X1/Z"
set tielo "LOGIC0_X1/Z"

restructure -liberty_file $liberty_library -target area -abc_logfile results/noise.log -tielo_port $tielo -tiehi_port $tiehi -work_dir ./results -cutremap true -so_cut true -head_pin $output_pin 

set verilog_file [make_result_file multilevel_op.v]
write_verilog $verilog_file
report_file $verilog_file


