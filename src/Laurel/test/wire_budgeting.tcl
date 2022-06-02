set_debug_level LRL "LRL" 1

source helpers.tcl

set test_nets ""

read_lef sky130hs/sky130hs.tlef 
read_lef sky130hs/sky130hs_std_cell.lef
read_liberty sky130hs/sky130hs_tt.lib
# read_verilog verilog/wire_bud.v
read_def gcd.def

# link_design top

# read_sdc sdc/function2.sdc
read_sdc sdc/wire_bud.sdc

initialize_floorplan \
      -utilization 20 \
      -aspect_ratio 1 \
      -core_space 1 \
      -site 1

# Load via resistance info
source sky130hs/sky130hs.rc
# source sky130hs_wire.rc

report_checks -to _671_/D

define_process_corner -ext_model_index 0 X
extract_parasitics -ext_model_file ext_pattern.rules \
      -max_res 0 -coupling_threshold 0.1

# report_checks -to _671_/D

set spef_file [make_result_file gcd.spef] 
write_spef $spef_file -nets $test_nets

# read_spef $spef_file


# define_process_corner -ext_model_index 0 X
# extract_parasitics -ext_model_file ext_pattern.rules \
#       -max_res 0 -coupling_threshold 0.1 -lef_res 2

# set spef_file [make_result_file gcd.spef] 
# write_spef $spef_file -nets $test_nets

# read_spef -coupling_reduction_factor 2 $spef_file

# report_checks -to _671_/D

# read_spef -coupling_reduction_factor 1.5 $spef_file

# report_checks -to _671_/D

# read_spef -coupling_reduction_factor 1 $spef_file

# report_checks -to _671_/D

# estimate_parasitics -placement
# laurel -setup
laurel -wire_budgeting -spef $spef_file
# laurel -setup
# repair_design

# report_checks

# set spef_file [make_result_file 45_gcd.spef] 
# write_spef $spef_file -nets $test_nets

# diff_files 45_gcd.spefok $spef_file
