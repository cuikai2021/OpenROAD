#pragma once

#include <vector>

namespace banyan_go {

#include "banyan_go_types.h"

struct go_slice
{
    const void *data;
    long long size;
    long long cap;
};

template <class T>
go_slice to_slice(const std::vector<T> &vec)
{
    return go_slice{vec.data(), (long long)vec.size(), (long long)vec.size()};
}

typedef long long (*entry_func)(go_slice boxes);

} // namespace banyan_go
