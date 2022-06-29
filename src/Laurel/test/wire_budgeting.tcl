set_debug_level LRL "LRL" 1

source helpers.tcl

set test_nets ""

read_lef sky130hs/sky130hs.tlef 
read_lef sky130hs/sky130hs_std_cell.lef
read_liberty sky130hs/sky130hs_tt.lib
read_def gcd.def

read_sdc sdc/wire_bud.sdc

# Load via resistance info
source sky130hs/sky130hs.rc

report_checks -to _671_/D

define_process_corner -ext_model_index 0 X
extract_parasitics -ext_model_file ext_pattern.rules \
      -max_res 0 -coupling_threshold 0.1

# report_checks -to _671_/D

set spef_file [make_result_file gcd.spef] 
write_spef $spef_file -nets $test_nets

# read_spef $spef_file

# read_spef -coupling_reduction_factor 2 $spef_file

# estimate_parasitics -placement
# laurel -setup
laurel -wire_budgeting -spef $spef_file -debug_report
# laurel -wire_budgeting -debug_report
# laurel -setup
# repair_design

# report_checks

