#cd /home/andy/Eden_restr_bug_fix/src/rmp/test
source "./helpers.tcl"
set liberty_library Nangate45/Nangate45_typ.lib
read_liberty $liberty_library
read_lef Nangate45/Nangate45.lef
read_verilog floploop.v
link_design floploop

set output_pin [get_pins "U3/Z"]

set tiehi "LOGIC1_X1/Z"
set tielo "LOGIC0_X1/Z"

restructure -liberty_file $liberty_library -target area -abc_logfile results/noise.log -tielo_port $tielo -tiehi_port $tiehi -work_dir ./results -cutremap -so_cut -head_pin $output_pin


write_verilog floploop_result.v
diff_files floploop_result.v floploop.vok
