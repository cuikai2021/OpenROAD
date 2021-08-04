#pragma once

#include <vector>
#include <stdlib.h>

namespace banyan_go {

#include "banyan_go_types.h"

template <class T>
struct go_slice
{
    T *data;
    long long size;
    long long cap;
}; // struct go_slice<T>

// There is no guarantee the go code won't modify the slice
// so let's assume the worst.
template <class T>
go_slice<T> to_slice(std::vector<T> &vec)
{
    return go_slice<T>{vec.data(), (long long)vec.size(), (long long)vec.size()};
}

// for banyan_version
typedef long long (*version_func)(const char *version);

// for banyan_route
typedef long long (*route_func)(go_slice<option> options,
    design dsgn, go_slice<layer> layers, go_slice<track> tracks,
    go_slice<net> nets, go_slice<pin> pins, go_slice<box> boxes, go_slice<via> vias,
    go_slice<viadef> viadefs, route_ret *ret);

// for banyan_route_with_axioms
typedef long long (*route_with_axioms_func)(go_slice<option> options,
    design dsgn, go_slice<layer> layers, go_slice<track> tracks,
    go_slice<net> nets, go_slice<pin> pins, go_slice<box> boxes, go_slice<via> vias,
    go_slice<viadef> viadefs, go_slice<axioms_triple> axioms_triples,
    go_slice<axioms_layer_table> axioms_via_track_tables,
    go_slice<axioms_layer_table> axioms_via_track_diff_tables,
    go_slice<axioms_layer_table> axioms_via_track_same_tables,
    go_slice<axioms_layer_table> axioms_trunk_via_tables,
    go_slice<axioms_layer_table> axioms_trunk_min_len_tables,
    go_slice<axioms_layer_table> axioms_trunk_dist_same_tables,
    go_slice<axioms_layer_table> axioms_trunk_dist_diff_tables,
    route_ret *ret);

} // namespace banyan_go
