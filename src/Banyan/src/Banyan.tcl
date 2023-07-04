
proc banyan { args } {
   banyan::banyan $args
}

proc banyan_run_gui {} {
  puts "banyan_run_gui"
  banyan::banyanRunGui
}

proc banyan_clear_wires {} {
  banyan::banyan_clear_wires
}

proc banyan_drc_patch {} {
   banyan::drcPatch
}

proc banyan_merge_wire_segment { } {
  banyan::mergeWireSegment
}
