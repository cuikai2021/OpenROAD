#pragma once

#include <optional>
#include <string>
#include <numeric>
#include <charconv>

// helper type for the visitor #4
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

// explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;


template<typename T = int>
auto getnum(std::string_view str)
{
    const auto fl = str.find_last_not_of(' ');	// Find end of data excluding trailing spaces

    if (fl == std::string_view::npos)	// If end of data not found, return no value
        return std::optional<T> {};

    const auto end = str.data() + fl + 1;	// End of data to be converted
    T num;

    return (std::from_chars(str.data() + str.find_first_not_of(' '), end, num).ptr == end) ? std::optional<T>{num} : std::optional<T> {};
}
