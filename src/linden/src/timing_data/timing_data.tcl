namespace eval linden {

sta::define_cmd_args "write_tdp_data" {\
    timing_data_file \
}

proc write_tdp_data {args} {
    linden::write_tdp_data_cmd $args
}

sta::define_cmd_args "read_tdp_data" {\
    timing_data_file \
}

proc read_tdp_data {args} {
    linden::read_tdp_data_cmd $args
}

}

namespace eval rsz {
# for historical reasons, keep these two APIs under rsz namespace.

sta::define_cmd_args "write_tdp_data" {\
    timing_data_file \
}

proc write_tdp_data {args} {
    linden::write_tdp_data_cmd $args
}

sta::define_cmd_args "read_tdp_data" {\
    timing_data_file \
}

proc read_tdp_data {args} {
    linden::read_tdp_data_cmd $args
}

}
