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
  banyan_diff_defs $out/nangate_test_same.def nangate_test.def
}

puts "\nTest udpate same def & db"

banyan_load_def nangate_test.def
diff_same
banyan_load_def nangate_test.def
diff_same

banyan_load_db nangate_test.db
diff_same

puts "\nTest wire change def & db"

banyan_load_def nangate_test_wire.def
write_def $out/nangate_test_change.def
banyan_diff_defs $out/nangate_test_change.def nangate_test.def

banyan_clear_wires
banyan_load_def nangate_test.def
diff_same
banyan_clear_wires
banyan_load_db nangate_test.db
diff_same

puts "\nTest nets pins ordering change"
banyan_load_def nangate_test_ordering_changed.def
diff_same
banyan_clear_wires
banyan_load_db nangate_test.db
diff_same

puts "\nTest inst change def"

catch {
banyan_load_def "nangate_test_error.def"
} error 

puts $error
