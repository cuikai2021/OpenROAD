

proc banyan { args } {
  banyan::banyan $args
}

proc inst_movement { args } {
   banyan::inst_movement $args
}
proc banyan_merge_wire_segment { } {
  banyan::mergeWireSegment
}

proc bymgm { } {
  set tech_lef in/tech.lef
  set lef_files [glob in/*lef]
  read_lef $tech_lef
  foreach lef_file $lef_files {
    if {[string equal $tech_lef $lef_file]} {
        continue
    }
    read_lef $lef_file
  }
  set def_files [glob in/*def]
  read_def [lindex $def_files 0]
  banyan::m1GridlessMaze
  write_def in/routed.def
  write_verilog in/routed.v
}

proc bymgm_path { args } {
  set tech_lef $args/tech.lef
  set lef_files [glob $args/*lef]
  read_lef $tech_lef
  foreach lef_file $lef_files {
    if {[string equal $tech_lef $lef_file]} {
        continue
    }
    read_lef $lef_file
  }
  set def_files [glob $args/*def]
  read_def [lindex $def_files 0]
  banyan::m1GridlessMaze
}

