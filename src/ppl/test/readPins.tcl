# gcd_nangate45 IO placement
source "helpers.tcl"
read_lef Nangate45/Nangate45.lef
read_def gcd.def

source readPins.cfg

set def_file [make_result_file readPins.def]

write_def $def_file

diff_file readPins.defok $def_file
