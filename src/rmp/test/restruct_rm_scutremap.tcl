#run in ./src/Laurel/test/restruct
source "./helpers.tcl"
#unit test for restructuring from a pin.
set test_path "../../rmp/test/cases/restr_unit/rm"
set liberty_library  $test_path/libs/ss/ua11lscef15bdrll_135c125_ss.lib
read_liberty $liberty_library
read_lef $test_path/lefs/ua11lscef15bdrll_6m2t.lef
read_lef $test_path/lefs/ua11lscef15bdrll.lef

read_verilog $test_path/rm_mapped.v
link_design rm
read_sdc $test_path/rm.sdc
source $test_path/setRC.tcl

set opts "-fields {slew cap input nets fanout} -max -digits 6"
set_delay_calculator simple_rc

report_checks -path_delay max -fields {slew cap input nets fanout} -format full_clock_expanded


set output_pin [get_pins "op"]

set tiehi "LOGIC1_X1/Z"
set tielo "LOGIC0_X1/Z"

restructure -liberty_file $liberty_library -target area -abc_logfile results/noise.log -tielo_port $tielo -tiehi_port $tiehi -work_dir ./results -cutremap -so_cut -head_pin $output_pin

report_checks -path_delay max -fields {slew cap input nets fanout} -format full_clock_expanded
write_verilog "./results/restruct_rm.v"
diff_files "./results/restruct_rm.v" "restruct_rm.vok"



