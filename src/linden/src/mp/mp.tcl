sta::define_cmd_args "linden_macro_placement" {\
    [-uniform_density] \
    [-density target_density]\
    [-overflow overflow]\
}

proc linden_macro_placement { args } {
    linden::StrDeque a
    foreach x $args {
        a push_back $x
    }
    linden::lindenmp_run_cmd a
}

