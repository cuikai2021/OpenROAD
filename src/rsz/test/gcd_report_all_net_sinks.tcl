# gcd full meal deal
read_liberty Nangate45/Nangate45_typ.lib
read_lef Nangate45/Nangate45.lef
read_def gcd_nangate45_placed.def
read_sdc gcd_nangate45.sdc

set_wire_rc -layer metal3
estimate_parasitics -placement

report_worst_slack -max -digits 7
rsz::report_all_sink_slacks

