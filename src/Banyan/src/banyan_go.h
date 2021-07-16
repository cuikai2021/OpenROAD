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

} // namespace banyan_go
