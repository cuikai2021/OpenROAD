# Ensure that running commands without loading a design doesn't crash.
source "helpers.tcl"

# make_port: parallaxsw/OpenSTA/issues/149
set skip {
  make_port
  exit_summary
  run_unit_test_and_exit
  vwait
  exit
  scan_chain_reorder
  scan_chain_trace
  linden_detailed_placement
  linden_incremental_legalize
  linden_macro_placement
  linden_routability_driven_legalize
  banyan_clear_wires
  pin_access
  laurel_get_clock_cells_name
  linden_remove_fillers
  test_lrl
  linden_global_placement
}

set arg [make_result_file commands_without_load]

foreach command [info commands] {
  puts "debug $command"
  if {[lsearch $skip $command] != -1} {
    puts "skip $command"
  } else {
    puts "TEST: $command"
    catch {$command} msg
    catch {$command $arg} msg
    catch {$command $arg $arg} msg
    catch {$command $arg $arg $arg} msg
    file delete $arg
  }
}

# If we got here without crashing then we passed
puts pass
