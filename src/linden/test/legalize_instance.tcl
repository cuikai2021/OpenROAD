source "helpers.tcl"
read_lef Nangate45.lef
read_lib Nangate45_typ.lib

read_def gcd_replace.def

set ::env(LINDEN_IN_REGRESSION) true

linden::legalize_instance _436_



