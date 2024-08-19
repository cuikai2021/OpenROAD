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