# exposing envflags

namespace eval linden::env {

proc tmp_file_root_dir {} {
    linden::env_tmp_file_root_dir
}

}

namespace eval linden {

proc eco_placer_find_vacant_places {eps buf_x2 p0 p1 x_delta y_delta} {
    set cpp_places [linden::eco_placer_find_vacant_places_i $eps $buf_x2 $p0 $p1 $x_delta $y_delta]
    set tcl_list {}
    set size [linden::vec_point_place_size $cpp_places]
    for {set i 0} {$i < $size} {incr i} {
        set item [linden::point_place_by_idx $cpp_places $i]
        lappend tcl_list $item
    }
    return $tcl_list
}

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