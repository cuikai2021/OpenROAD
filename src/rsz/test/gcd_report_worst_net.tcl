# gcd full meal deal
read_liberty Nangate45/Nangate45_typ.lib
read_lef Nangate45/Nangate45.lef
read_def gcd_nangate45_placed.def
read_sdc gcd_nangate45.sdc

create_clock -period 0.3 clk

set_wire_rc -layer metal3
estimate_parasitics -placement

report_worst_slack -max -digits 7
rsz::report_worst_slack_net 1

rsz::report_worst_slack_net 10
