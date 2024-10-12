source "helpers.tcl"
source Nangate45/Nangate45.vars

set out "results"

suppress_message BY 1
read_lef $tech_lef
read_lef $std_cell_lef

read_def nangate_test.def

proc diff_same {} {
  global out
  write_def $out/nangate_test_same.def
}

puts "\nTest udpate same def & db"

banyan_load_def nangate_test.def
diff_same

