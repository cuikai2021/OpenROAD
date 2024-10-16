# exposing envflags

namespace eval linden::env {

proc tmp_file_root_dir {} {
    linden::env_tmp_file_root_dir
}

}

namespace eval linden {

proc eco_placer_find_vacant_places {eps instance p0 p1 x_delta y_delta} {
    set cpp_places [linden::eco_placer_find_vacant_places_i $eps $instance $p0 $p1 $x_delta $y_delta]
    set tcl_list {}
    set size [linden::vec_point_place_size $cpp_places]
    for {set i 0} {$i < $size} {incr i} {
        set item [linden::point_place_by_idx $cpp_places $i]
        lappend tcl_list $item
    }
    return $tcl_list
}

proc eco_placer_find_nearest_place {args} {
    if {[llength $args] < 3} {
        error "Usage: eco_placer_find_nearest_place eps instance point ?--xmin value? ?--ymin value? ?--xmax value? ?--ymax value?"
    }

    set eps [lindex $args 0]
    set instance [lindex $args 1]
    set point [lindex $args 2]

    array set opts {xmin "null" ymin "null" xmax "null" ymax "null"}

    set i 3
    while {$i < [llength $args]} {
        set option [lindex $args $i]
        switch -- $option {
            "--xmin" {
                incr i
                if {$i >= [llength $args]} { error "Missing value for --xmin" }
                set opts(xmin) [lindex $args $i]
            }
            "--ymin" {
                incr i
                if {$i >= [llength $args]} { error "Missing value for --ymin" }
                set opts(ymin) [lindex $args $i]
            }
            "--xmax" {
                incr i
                if {$i >= [llength $args]} { error "Missing value for --xmax" }
                set opts(xmax) [lindex $args $i]
            }
            "--ymax" {
                incr i
                if {$i >= [llength $args]} { error "Missing value for --ymax" }
                set opts(ymax) [lindex $args $i]
            }
            default {
                error "Unknown option: $option"
            }
        }
        incr i
    }

    set pi [linden::eco_placer_find_nearest_place_i $eps $instance $point $opts(xmin) $opts(ymin) $opts(xmax) $opts(ymax)]

    return $pi
}

# end namespace linden
} 

sta::define_cmd_args "add_tie_cell" {
    [--tie-cell tie-low-cell low-pin tie-high-cell high-pin]
    [--max-fanout max-fanout]
    [--max-distance max-distance]
    [--suppress-output]
    [--not-to-stop-when-illegal]
}

proc linden_add_tiecell { args } {
    linden::StrDeque a
    foreach x $args {
        a push_back $x
    }
    linden::add_tie_cell_cmd a
}