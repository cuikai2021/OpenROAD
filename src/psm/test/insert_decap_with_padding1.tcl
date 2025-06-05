# insert_decap for sky130hd/gcd
source "helpers.tcl"

read_liberty sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib
read_lef "sky130hd/sky130hd.tlef"
read_lef "sky130hd/sky130hd_std_cell.lef"
read_def "sky130hd_data/insert_decap_gcd.def"

source sky130hd/sky130hd.rc

analyze_power_grid -net {VDD}

# Set padding before insert decap cells
set_placement_padding -global -left 1 -right 1

insert_decap -net VDD -targets { ( 6.440, 29.920 ) 1.5 ( 32.200, 59.920 ) 10 (58.420, 2.720) 2 } -cells {"sky130_fd_sc_hd__decap_3" 0.93 "sky130_fd_sc_hd__decap_4" 0.124 "sky130_fd_sc_hd__decap_6" 0.186 "sky130_fd_sc_hd__decap_8" 0.248 "sky130_fd_sc_hd__decap_12" 0.362}  -h 2 -v 2

insert_decap -net VDD -region_count {3 3} -region_target 5  -cells {"sky130_fd_sc_hd__decap_3" 0.93 "sky130_fd_sc_hd__decap_4" 0.124 "sky130_fd_sc_hd__decap_6" 0.186 "sky130_fd_sc_hd__decap_8" 0.248 "sky130_fd_sc_hd__decap_12" 0.362}

# Reset padding to check placement
set_placement_padding -global -left 0 -right 0

check_placement

set def_file [make_result_file insert_decap_with_padding1.def]
write_def $def_file

diff_file $def_file insert_decap_with_padding1.defok
