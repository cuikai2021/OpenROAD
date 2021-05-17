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

typedef long long (*entry_func)(
    go_slice<net> nets, go_slice<pin> pins, go_slice<box> boxes,
    go_slice<layer> layers, go_slice<track> tracks,
    entry_ret *ret);

} // namespace banyan_go
