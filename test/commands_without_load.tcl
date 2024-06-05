# Ensure that running commands without loading a design doesn't crash.

set skip {
  define_corners
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

foreach command [info commands] {
  puts "debug $command"
  if {[lsearch $skip $command] != -1} {
    puts "skip $command"
  } else {
    catch {$command} msg
  }
}

# If we got here without crashing then we passed
puts pass
