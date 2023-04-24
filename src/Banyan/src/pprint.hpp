/*
 _______  _______  ______    ___   __    _  _______
|       ||       ||    _ |  |   | |  |  | ||       |
|    _  ||    _  ||   | ||  |   | |   |_| ||_     _|
|   |_| ||   |_| ||   |_||_ |   | |       |  |   |
|    ___||    ___||    __  ||   | |  _    |  |   |
|   |    |   |    |   |  | ||   | | | |   |  |   |
|___|    |___|    |___|  |_||___| |_|  |__|  |___|
Pretty Printer for Modern C++
https://github.com/p-ranav/pprint
Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2019 Pranav Srinivas Kumar <pranav.srinivas.kumar@gmail.com>.
Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once
#include <iostream>
#include <string>
#include <typeinfo>
#include <type_traits>
#include <vector>
#include <list>
#include <deque>
#include <set>
#include <unordered_set>
#include <array>
#include <map>
#include <unordered_map>
#include <iomanip>
#include <variant>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iosfwd>
#include <limits>
#include <string_view>
#include <optional>
#include <utility>
#include <sstream>
#include <queue>
#include <stack>
#include <tuple>
#include <initializer_list>
#include <complex>
#include <cmath>
#include <memory>
#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h>
#endif

#include "magic_enum.hpp"

// Check if a type is stream writable, i.e., std::cout << foo;
template<typename S, typename T, typename = void>
struct is_to_stream_writable: std::false_type {};

template<typename S, typename T>
struct is_to_stream_writable<S, T,
           std::void_t<  decltype( std::declval<S&>()<<std::declval<T>() ) >>
  : std::true_type {};


// Printing std::tuple
// The indices trick: http://loungecpp.wikidot.com/tips-and-tricks:indices
namespace pprint {

  template<std::size_t...> struct seq{};

  template<std::size_t N, std::size_t... Is>
  struct gen_seq : gen_seq<N-1, N-1, Is...>{};

  template<std::size_t... Is>
  struct gen_seq<0, Is...> : seq<Is...>{};

  template<typename T>
  inline T to_string(T value) {
    return value;
  }

  inline std::string to_string(char value) {
    return "'" + std::string(1, value) + "'";
  }

  inline std::string to_string(const char * value) {
    return "\"" + std::string(value) + "\"";
  }

  inline std::string to_string(const std::string& value) {
    return "\"" + value + "\"";
  }

  template<class Ch, class Tr, class Tuple, std::size_t... Is>
  void print_tuple(std::basic_ostream<Ch,Tr>& os, Tuple const& t, seq<Is...>){
    using swallow = int[];
    (void)swallow{0, (void(os << (Is == 0? "" : ", ") << to_string(std::get<Is>(t))), 0)...};
  }

}

template<class Ch, class Tr, class... Args>
auto operator<<(std::basic_ostream<Ch, Tr>& os, std::tuple<Args...> const& t)
  -> std::basic_ostream<Ch, Tr>& {
  os << "(";
  pprint::print_tuple(os, t, pprint::gen_seq<sizeof...(Args)>());
  return os << ")";
}

namespace pprint {

  // Some utility structs to check template specialization
  template<typename Test, template<typename...> class Ref>
  struct is_specialization : std::false_type {};

  template<template<typename...> class Ref, typename... Args>
  struct is_specialization<Ref<Args...>, Ref> : std::true_type {};

  template<typename ...>
  using to_void = void;

  template<typename T, typename = void>
  struct is_container : std::false_type
  {};

  template<typename T>
  struct is_container<T,
          to_void<decltype(std::declval<T>().begin()),
              decltype(std::declval<T>().end()),
              typename T::value_type
          >> : std::true_type // will  be enabled for iterable objects
  {};

  class PrettyPrinter {
  private:
    std::ostream& stream_;
    std::string line_terminator_;
    size_t indent_;
    bool quotes_;
    bool compact_;

  public:

    PrettyPrinter(std::ostream& stream = std::cout) :
      stream_(stream),
      line_terminator_("\n"),
      indent_(2),
      quotes_(false),
      compact_(false) {}

    PrettyPrinter& line_terminator(const std::string& value) {
      line_terminator_ = value;
      return *this;
    }

    PrettyPrinter& indent(size_t indent) {
      indent_ = indent;
      return *this;
    }

    PrettyPrinter& compact(bool value) {
      compact_ = value;
      return *this;
    }

    PrettyPrinter& quotes(bool value) {
      quotes_ = value;
      return *this;
    }

    template <typename T>
    void print(T value) {
      print_internal(value, 0, line_terminator_, 0);
    }

    template <typename T>
    void print(std::initializer_list<T> value) {
      print_internal(value, 0, line_terminator_, 0);
    }

    template<typename T, typename... Targs>
    void print(T value, Targs... Fargs) {
      print_internal(value, 0, "", 0);
      auto current_quotes = quotes_;
      quotes_ = false;
      print_internal(" ", 0, "", 0);
      quotes_ = current_quotes;
      print(Fargs...);
    }

    template <typename T>
    void print_inline(T value) {
      print_internal(value, indent_, "", 0);
    }

    template <typename T>
    void print_inline(std::initializer_list<T> value) {
      print_internal(value, indent_, "", 0);
    }

    template<typename T, typename... Targs>
    void print_inline(T value, Targs... Fargs) {
      print_internal(value, indent_, "", 0);
      auto current_quotes = quotes_;
      quotes_ = false;
      print_internal(" ", 0, "", 0);
      quotes_ = current_quotes;
      print_inline(Fargs...);
    }

  private:

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value == true, void>::type
    print_internal(T value, size_t indent = 0, const std::string& line_terminator = "\n", size_t level = 0) {
      stream_ << std::string(indent, ' ') << value << line_terminator;
    }

    template <typename T>
    typename std::enable_if<std::is_null_pointer<T>::value == true, void>::type
    print_internal(T value, size_t indent = 0, const std::string& line_terminator = "\n", size_t level = 0) {
      stream_ << std::string(indent, ' ') << "nullptr" << line_terminator;
    }

    void print_internal(float value, size_t indent = 0, const std::string& line_terminator = "\n", size_t level = 0) {
      stream_ << std::string(indent, ' ') << value << 'f' << line_terminator;
    }

    void print_internal(double value, size_t indent = 0, const std::string& line_terminator = "\n", size_t level = 0) {
      stream_ << std::string(indent, ' ') << value << line_terminator;
    }

    void print_internal(const std::string& value, size_t indent = 0, const std::string& line_terminator = "\n",
      size_t level = 0) {
      if (!quotes_)
        print_internal_without_quotes(value, indent, line_terminator, level);
      else
        stream_ << std::string(indent, ' ') << "\"" << value << "\"" << line_terminator;
    }

    void print_internal(const char * value, size_t indent = 0, const std::string& line_terminator = "\n",
      size_t level = 0) {
      if (!quotes_)
        print_internal_without_quotes(value, indent, line_terminator, level);
      else
        stream_ << std::string(indent, ' ') << "\"" << value << "\"" << line_terminator;
    }

    void print_internal(char value, size_t indent = 0, const std::string& line_terminator = "\n", size_t level = 0) {
      if (!quotes_)
        print_internal_without_quotes(value, indent, line_terminator, level);
      else
        stream_ << std::string(indent, ' ') << "'" << value << "'" << line_terminator;
    }

    void print_internal_without_quotes(const std::string& value, size_t indent = 0,
      const std::string& line_terminator = "\n", size_t level = 0) {
      stream_ << std::string(indent, ' ') << value << line_terminator;
    }

    void print_internal_without_quotes(const char * value, size_t indent = 0,
      const std::string& line_terminator = "\n", size_t level = 0) {
      stream_ << std::string(indent, ' ') << value << line_terminator;
    }

    void print_internal_without_quotes(char value, size_t indent = 0, const std::string& line_terminator = "\n",
      size_t level = 0) {
      stream_ << std::string(indent, ' ') << value << line_terminator;
    }

    void print_internal(bool value, size_t indent = 0, const std::string& line_terminator = "\n", size_t level = 0) {
      stream_ << std::string(indent, ' ') << (value ? "true" : "false") << line_terminator;
    }

    template <typename T>
    typename std::enable_if<std::is_pointer<T>::value == true, void>::type
    print_internal(T value, size_t indent = 0, const std::string& line_terminator = "\n", size_t level = 0) {
      if (value == nullptr) {
        return print_internal(nullptr, indent, line_terminator, level);
      }
      stream_ << std::string(indent, ' ') << "<" << type(value) << " at "
              << value << ">" << line_terminator;
    }

    std::string demangle(const char* name) {
#ifdef __GNUG__
      int status = -4;
      std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle(name, NULL, NULL, &status),
        std::free
      };
      return (status==0) ? res.get() : name;
#else
      return name;
#endif
    }

    template <class T>
    std::string type(const T& t) {
      return demangle(typeid(t).name());
    }

    template <typename T>
    typename std::enable_if<std::is_enum<T>::value == true, void>::type
    print_internal(T value, size_t indent = 0, const std::string& line_terminator = "\n", size_t level = 0) {
      auto enum_string = magic_enum::enum_name(value);
      if (enum_string.has_value()) {
        stream_ << std::string(indent, ' ') << enum_string.value()
                << line_terminator;
      }
      else {
        stream_ << std::string(indent, ' ') << static_cast<std::underlying_type_t<T>>(value)
                << line_terminator;
      }
    }

    template <typename T>
    typename std::enable_if<std::is_class<T>::value == true &&
        is_to_stream_writable<std::ostream, T>::value == true &&
        std::is_enum<T>::value == false &&
        is_specialization<T, std::unique_ptr>::value == false &&
        is_specialization<T, std::shared_ptr>::value == false &&
        is_specialization<T, std::weak_ptr>::value == false &&
        is_specialization<T, std::tuple>::value == false &&
        is_specialization<T, std::variant>::value == false &&
        is_specialization<T, std::vector>::value == false &&
        is_specialization<T, std::list>::value == false &&
        is_specialization<T, std::deque>::value == false &&
        is_specialization<T, std::queue>::value == false &&
        is_specialization<T, std::priority_queue>::value == false &&
        is_specialization<T, std::stack>::value == false &&
        is_specialization<T, std::set>::value == false &&
        is_specialization<T, std::multiset>::value == false &&
        is_specialization<T, std::unordered_set>::value == false &&
        is_specialization<T, std::unordered_multiset>::value == false &&
        is_specialization<T, std::map>::value == false &&
        is_specialization<T, std::multimap>::value == false &&
        is_specialization<T, std::unordered_map>::value == false &&
        is_specialization<T, std::unordered_multimap>::value == false, void>::type
    print_internal(T value, size_t indent = 0, const std::string& line_terminator = "\n", size_t level = 0) {
      stream_ << std::string(indent, ' ') << value << line_terminator;
    }

    template <typename T>
    typename std::enable_if<std::is_class<T>::value == true &&
            is_to_stream_writable<std::ostream, T>::value == false &&
            std::is_enum<T>::value == false &&
            is_specialization<T, std::unique_ptr>::value == false &&
            is_specialization<T, std::shared_ptr>::value == false &&
            is_specialization<T, std::weak_ptr>::value == false &&
            is_specialization<T, std::tuple>::value == false &&
            is_specialization<T, std::variant>::value == false &&
            is_specialization<T, std::vector>::value == false &&
            is_specialization<T, std::list>::value == false &&
            is_specialization<T, std::deque>::value == false &&
            is_specialization<T, std::queue>::value == false &&
            is_specialization<T, std::priority_queue>::value == false &&
            is_specialization<T, std::stack>::value == false &&
            is_specialization<T, std::set>::value == false &&
            is_specialization<T, std::multiset>::value == false &&
            is_specialization<T, std::unordered_set>::value == false &&
            is_specialization<T, std::unordered_multiset>::value == false &&
            is_specialization<T, std::map>::value == false &&
            is_specialization<T, std::multimap>::value == false &&
            is_specialization<T, std::unordered_map>::value == false &&
            is_specialization<T, std::unordered_multimap>::value == false, void>::type
            print_internal(T value, size_t indent = 0, const std::string& line_terminator = "\n", size_t level = 0) {
      stream_ << std::string(indent, ' ') << "<Object " << type(value) << ">"
              << line_terminator;
    }

    template <typename T>
    typename std::enable_if<std::is_member_function_pointer<T>::value == true, void>::type
    print_internal(T value, size_t indent = 0, const std::string& line_terminator = "\n", size_t level = 0) {
      stream_ << std::string(indent, ' ') << "<Object.method " << type(value)
              << " at " << &value << ">"
              << line_terminator;
    }

    template <typename Container>
    typename std::enable_if<is_specialization<Container, std::vector>::value, void>::type
            print_internal(const Container& value, size_t indent = 0, const std::string& line_terminator = "\n",
            size_t level = 0) {
      typedef typename Container::value_type T;
      if (level == 0 && !compact_) {
        if (value.size() == 0) {
          print_internal_without_quotes("[", 0, "");
        }
        else if (value.size() == 1) {
          print_internal_without_quotes("[", 0, "");
          print_internal(value.front(), 0, "", level + 1);
        }
        else if (value.size() > 0) {
          print_internal_without_quotes("[", 0, "\n");
          print_internal(value.front(), indent + indent_, "", level + 1);
          if (value.size() > 1 && is_container<T>::value == false)
            print_internal_without_quotes(", ", 0, "\n");
          else if (is_container<T>::value)
            print_internal_without_quotes(", ", 0, "\n");
          for (size_t i = 1; i < value.size() - 1; i++) {
            print_internal(value[i], indent + indent_, "", level + 1);
            if (is_container<T>::value == false)
              print_internal_without_quotes(", ", 0, "\n");
            else
              print_internal_without_quotes(", ", 0, "\n");
          }
          if (value.size() > 1) {
            print_internal(value.back(), indent + indent_, "\n", level + 1);
          }
        }
        if (value.size() == 0)
          print_internal_without_quotes("]", indent, "");
        else if (is_container<T>::value == false)
          print_internal_without_quotes("]", indent, "");
        else
          print_internal_without_quotes(line_terminator_ + "]", indent, "");
        print_internal_without_quotes(line_terminator_, 0, "");
      }
      else {
        if (value.size() == 0) {
          print_internal_without_quotes("[", indent, "");
        }
        else if (value.size() == 1) {
          print_internal_without_quotes("[", indent, "");
          print_internal(value.front(), 0, "", level + 1);
        }
        else if (value.size() > 0) {
          print_internal_without_quotes("[", indent, "");
          print_internal(value.front(), 0, "", level + 1);
          if (value.size() > 1)
            print_internal_without_quotes(", ", 0, "");
          for (size_t i = 1; i < value.size() - 1; i++) {
            print_internal(value[i], 0, "", level + 1);
            print_internal_without_quotes(", ", 0, "");
          }
          if (value.size() > 1) {
            print_internal(value.back(), 0, "", level + 1);
          }
        }
        print_internal_without_quotes("]", 0, "");
        if (level == 0 && compact_)
          print_internal_without_quotes(line_terminator_, 0, "");
      }

    }

    template <typename T, unsigned long int S>
    void print_internal(const std::array<T, S>& value, size_t indent = 0, const std::string& line_terminator = "\n",
            size_t level = 0) {
      if (level == 0 && !compact_) {
        if (value.size() == 0) {
          print_internal_without_quotes("[", 0, "");
        }
        else if (value.size() == 1) {
          print_internal_without_quotes("[", 0, "");
          print_internal(value.front(), 0, "", level + 1);
        }
        else if (value.size() > 0) {
          print_internal_without_quotes("[", 0, "\n");
          print_internal(value.front(), indent + indent_, "", level + 1);
          if (value.size() > 1 && is_container<T>::value == false)
            print_internal_without_quotes(", ", 0, "\n");
          else if (is_container<T>::value)
            print_internal_without_quotes(", ", 0, "\n");
          for (size_t i = 1; i < value.size() - 1; i++) {
            print_internal(value[i], indent + indent_, "", level + 1);
            if (is_container<T>::value == false)
              print_internal_without_quotes(", ", 0, "\n");
            else
              print_internal_without_quotes(", ", 0, "\n");
          }
          if (value.size() > 1) {
            print_internal(value.back(), indent + indent_, "\n", level + 1);
          }
        }
        if (value.size() == 0)
          print_internal_without_quotes("]", indent, "");
        else if (is_container<T>::value == false)
          print_internal_without_quotes("]", indent, "");
        else
          print_internal_without_quotes(line_terminator_ + "]", indent, "");
        print_internal_without_quotes(line_terminator_, 0, "");
      }
      else {
        if (value.size() == 0) {
          print_internal_without_quotes("[", indent, "");
        }
        else if (value.size() == 1) {
          print_internal_without_quotes("[", indent, "");
          print_internal(value.front(), 0, "", level + 1);
        }
        else if (value.size() > 0) {
          print_internal_without_quotes("[", indent, "");
          print_internal(value.front(), 0, "", level + 1);
          if (value.size() > 1)
            print_internal_without_quotes(", ", 0, "");
          for (size_t i = 1; i < value.size() - 1; i++) {
            print_internal(value[i], 0, "", level + 1);
            print_internal_without_quotes(", ", 0, "");
          }
          if (value.size() > 1) {
            print_internal(value.back(), 0, "", level + 1);
          }
        }
        print_internal_without_quotes("]", 0, "");
        if (level == 0 && compact_)
          print_internal_without_quotes(line_terminator_, 0, "");
      }

    }

    template <typename Container>
    typename std::enable_if<is_specialization<Container, std::list>::value ||
            is_specialization<Container, std::deque>::value,
            void>::type print_internal(const Container& value, size_t indent = 0,
            const std::string& line_terminator = "\n",
            size_t level = 0) {
      typedef typename Container::value_type T;
      if (level == 0 && !compact_) {
        if (value.size() == 0) {
          print_internal_without_quotes("[", 0, "");
        }
        else if (value.size() == 1) {
          print_internal_without_quotes("[", 0, "");
          print_internal(value.front(), 0, "", level + 1);
        }
        else if (value.size() > 0) {
          print_internal_without_quotes("[", 0, "\n");
          print_internal(value.front(), indent + indent_, "", level + 1);
          if (value.size() > 1 && is_container<T>::value == false)
            print_internal_without_quotes(", ", 0, "\n");
          else if (is_container<T>::value)
            print_internal_without_quotes(", ", 0, "\n");

          typename Container::const_iterator iterator;
          for (iterator = std::next(value.begin()); iterator != std::prev(value.end()); ++iterator) {
            print_internal(*iterator, indent + indent_, "", level + 1);
            if (is_container<T>::value == false)
              print_internal_without_quotes(", ", 0, "\n");
            else
              print_internal_without_quotes(", ", 0, "\n");
          }

          if (value.size() > 1) {
            print_internal(value.back(), indent + indent_, "\n", level + 1);
          }
        }
        if (value.size() == 0)
          print_internal_without_quotes("]", indent, "");
        else if (is_container<T>::value == false)
          print_internal_without_quotes("]", indent, "");
        else
          print_internal_without_quotes(line_terminator_ + "]", indent, "");
        print_internal_without_quotes(line_terminator_, 0, "");
      }
      else {
        if (value.size() == 0) {
          print_internal_without_quotes("[", indent, "");
        }
        else if (value.size() == 1) {
          print_internal_without_quotes("[", indent, "");
          print_internal(value.front(), 0, "", level + 1);
        }
        else if (value.size() > 0) {
          print_internal_without_quotes("[", indent, "");
          print_internal(value.front(), 0, "", level + 1);
          if (value.size() > 1)
            print_internal_without_quotes(", ", 0, "");

          typename Container::const_iterator iterator;
          for (iterator = std::next(value.begin()); iterator != std::prev(value.end()); ++iterator) {
            print_internal(*iterator, 0, "", level + 1);
            print_internal_without_quotes(", ", 0, "");
          }

          if (value.size() > 1) {
            print_internal(value.back(), 0, "", level + 1);
          }
        }
        print_internal_without_quotes("]", 0, "");
        if (level == 0 && compact_)
          print_internal_without_quotes(line_terminator_, 0, "");
      }

    }

    template <typename Container>
    typename std::enable_if<is_specialization<Container, std::set>::value ||
            is_specialization<Container, std::multiset>::value ||
            is_specialization<Container, std::unordered_set>::value ||
            is_specialization<Container, std::unordered_multiset>::value, void>::type
            print_internal(const Container& value, size_t indent = 0, const std::string& line_terminator = "\n",
            size_t level = 0) {
      typedef typename Container::value_type T;
      if (level == 0 && !compact_) {
        if (value.size() == 0) {
          print_internal_without_quotes("{", 0, "");
        }
        else if (value.size() == 1) {
          print_internal_without_quotes("{", 0, "");
          print_internal(*(value.begin()), 0, "", level + 1);
        }
        else {
          print_internal_without_quotes("{", 0, "\n");
          print_internal(*(value.begin()), indent + indent_, "", level + 1);
          if (value.size() > 1 && is_container<T>::value == false)
            print_internal_without_quotes(", ", 0, "\n");
          else if (is_container<T>::value)
            print_internal_without_quotes(", ", 0, "\n");

          typename Container::const_iterator iterator;
          for (iterator = std::next(value.begin()); (iterator != value.end()) && (std::next(iterator) != value.end()); ++iterator) {
            print_internal(*iterator, indent + indent_, "", level + 1);
            if (is_container<T>::value == false)
              print_internal_without_quotes(", ", 0, "\n");
            else
              print_internal_without_quotes(", ", 0, "\n");
          }

          if (value.size() > 1) {
            print_internal(*iterator, indent + indent_, "\n", level + 1);
          }
        }
        if (value.size() == 0)
          print_internal_without_quotes("}", indent, "");
        else if (is_container<T>::value == false)
          print_internal_without_quotes("}", indent, "");
        else
          print_internal_without_quotes(line_terminator_ + "}", indent, "");
        print_internal_without_quotes(line_terminator_, 0, "");
      }
      else {
        if (value.size() == 0) {
          print_internal_without_quotes("{", indent, "");
        }
        else if (value.size() == 1) {
          print_internal_without_quotes("{", indent, "");
          print_internal(*(value.begin()), 0, "", level + 1);
        }
        else {
          print_internal_without_quotes("{", indent, "");
          print_internal(*(value.begin()), 0, "", level + 1);
          if (value.size() > 1)
            print_internal_without_quotes(", ", 0, "");

          typename Container::const_iterator iterator;
          for (iterator = std::next(value.begin()); (iterator != value.end()) && (std::next(iterator) != value.end()); ++iterator) {
            print_internal(*iterator, 0, "", level + 1);
            print_internal_without_quotes(", ", 0, "");
          }

          if (value.size() > 1) {
            print_internal(*iterator, 0, "", level + 1);
          }
        }
        print_internal_without_quotes("}", 0, "");
        if (level == 0 && compact_)
          print_internal_without_quotes(line_terminator_, 0, "");
      }

    }

    template <typename T>
    typename std::enable_if<is_specialization<T, std::map>::value == true ||
            is_specialization<T, std::multimap>::value == true ||
            is_specialization<T, std::unordered_map>::value == true ||
            is_specialization<T, std::unordered_multimap>::value == true, void>::type
            print_internal(const T& value, size_t indent = 0, const std::string& line_terminator = "\n", size_t level = 0) {
      typedef typename T::mapped_type Value;
      if (level == 0 && !compact_) {
        if (value.size() == 0) {
          print_internal_without_quotes("{", 0, "");
        }
        else if (value.size() == 1) {
          print_internal_without_quotes("{", 0, "");
          for (auto& kvpair : value) {
            print_internal(kvpair.first, 0, "", level + 1);
            print_internal_without_quotes(" : ", 0, "");
            print_internal(kvpair.second, 0, "", level + 1);
          }
        }
        else if (value.size() > 0) {
          size_t count = 0;
          for (auto& kvpair : value) {
            if (count == 0) {
              print_internal_without_quotes("{", 0, "\n");
              print_internal(kvpair.first, indent + indent_, "", level + 1);
              print_internal_without_quotes(" : ", 0, "");
              print_internal(kvpair.second, 0, "", level + 1);
              if (value.size() > 1 && is_container<Value>::value == false)
                print_internal_without_quotes(", ", 0, "\n");
              else if (is_container<Value>::value)
                print_internal_without_quotes(", ", 0, "\n");
            }
            else if (count + 1 < value.size()) {
              print_internal(kvpair.first, indent + indent_, "", level + 1);
              print_internal_without_quotes(" : ", 0, "");
              print_internal(kvpair.second, 0, "", level + 1);
              if (is_container<Value>::value == false)
                print_internal_without_quotes(", ", 0, "\n");
              else
                print_internal_without_quotes(", ", 0, "\n");
            }
            else {
              print_internal(kvpair.first, indent + indent_, "", level + 1);
              print_internal_without_quotes(" : ", 0, "");
              print_internal(kvpair.second, 0, "\n", level + 1);
            }
            count += 1;
          }
        }
        if (value.size() == 0)
          print_internal_without_quotes("}", indent, "");
        else if (is_container<Value>::value == false)
          print_internal_without_quotes("}", indent, "");
        else
          print_internal_without_quotes(line_terminator_ + "}", indent, "");
        print_internal_without_quotes(line_terminator_, 0, "");
      }

      else {
        if (value.size() == 0) {
          print_internal_without_quotes("{", indent, "");
        }
        else if (value.size() == 1) {
          print_internal_without_quotes("{", indent, "");
          for (auto& kvpair : value) {
            print_internal(kvpair.first, 0, "", level + 1);
            print_internal_without_quotes(" : ", 0, "");
            print_internal(kvpair.second, 0, "", level + 1);
          }
        }
        else if (value.size() > 0) {
          size_t count = 0;
          for (auto& kvpair : value) {
            if (count == 0) {
              print_internal_without_quotes("{", indent, "");
              print_internal(kvpair.first, 0, "", level + 1);
              print_internal_without_quotes(" : ", 0, "");
              print_internal(kvpair.second, 0, "", level + 1);
              print_internal_without_quotes(", ", 0, "");
            }
            else if (count + 1 < value.size()) {
              print_internal(kvpair.first, indent + indent_, "", level + 1);
              print_internal_without_quotes(" : ", 0, "");
              print_internal(kvpair.second, 0, "", level + 1);
              print_internal_without_quotes(", ", 0, "");
            }
            else {
              print_internal(kvpair.first, 0, "", level + 1);
              print_internal_without_quotes(" : ", 0, "");
              print_internal(kvpair.second, 0, "", level + 1);
            }
            count += 1;
          }
        }
        print_internal_without_quotes("}", 0, "");
        if (level == 0 && compact_)
          print_internal_without_quotes(line_terminator_, 0, "");
      }
    }

    template <typename Key, typename Value>
    void print_internal(std::pair<Key, Value> value, size_t indent = 0, const std::string& line_terminator = "\n",
            size_t level = 0) {
      print_internal_without_quotes("(", indent, "");
      print_internal(value.first, 0, "");
      print_internal_without_quotes(", ", 0, "");
      print_internal(value.second, 0, "");
      print_internal_without_quotes(")", 0, line_terminator, level);
    }

    template <class ...Ts>
    void print_internal(std::variant<Ts...> value, size_t indent = 0,
        const std::string& line_terminator = "\n", size_t level = 0) {
      std::visit([=, this](const auto& value) { print_internal(value, indent, line_terminator, level); }, value);
    }

    template <typename T>
    void print_internal(std::optional<T> value, size_t indent = 0,
        const std::string& line_terminator = "\n", size_t level = 0) {
      if (value) {
        print_internal(value.value(), indent, line_terminator, level);
      }
      else {
        print_internal_without_quotes("nullopt", indent, line_terminator, level);
      }
    }

    template <typename Container>
    typename std::enable_if<is_specialization<Container, std::queue>::value, void>::type
            print_internal(const Container& value, size_t indent = 0, const std::string& line_terminator = "\n",
            size_t level = 0) {
      auto current_compact = compact_;
      compact_ = true;
      typedef typename Container::value_type T;
      auto local = value;
      std::vector<T> local_vector;
      while (!local.empty()) {
        local_vector.push_back(local.front());
        local.pop();
      }
      print_internal(local_vector, indent, line_terminator, level);
      compact_ = current_compact;
    }

    template <typename Container>
    typename std::enable_if<is_specialization<Container, std::priority_queue>::value, void>::type
    print_internal(const Container& value, size_t indent = 0, const std::string& line_terminator = "\n",
            size_t level = 0) {
      auto current_compact = compact_;
      compact_ = true;
      typedef typename Container::value_type T;
      auto local = value;
      std::vector<T> local_vector;
      while (!local.empty()) {
        local_vector.push_back(local.top());
        local.pop();
      }
      print_internal(local_vector, indent, line_terminator, level);
      compact_ = current_compact;
    }

    template <typename T>
    void print_internal(std::initializer_list<T> value, size_t indent = 0,
            const std::string& line_terminator = "\n", size_t level = 0) {
      std::multiset<T> local;
      for(const T& x : value) {
        local.insert(x);
      }
      print_internal(local, indent, line_terminator_, level);
    }

    template <typename Container>
    typename std::enable_if<is_specialization<Container, std::stack>::value, void>::type
    print_internal(const Container& value, size_t indent = 0, const std::string& line_terminator = "\n",
            size_t level = 0) {
      bool current_compact = compact_;
      compact_ = false; // Need to print a stack like its a stack, i.e., vertical
      typedef typename Container::value_type T;
      auto local = value;
      std::vector<T> local_vector;
      while (!local.empty()) {
        local_vector.push_back(local.top());
        local.pop();
      }
      print_internal(local_vector, indent, line_terminator, level);
      compact_ = current_compact;
    }

    template<class... Args>
    void print_internal(const std::tuple<Args...>& value, size_t indent = 0, const std::string& line_terminator = "\n",
            size_t level = 0) {
      stream_ << std::string(indent, ' ') << value
              << line_terminator;
    }

    template<typename T>
    void print_internal(const std::complex<T>& value, size_t indent = 0, const std::string& line_terminator = "\n",
        size_t level = 0) {
      stream_ << std::string(indent, ' ') << "(" <<
      value.real() << " + " << value.imag() << "i)"
                   << line_terminator;
    }

    template<typename Pointer>
    typename std::enable_if<is_specialization<Pointer, std::unique_ptr>::value ||
        is_specialization<Pointer, std::shared_ptr>::value ||
        is_specialization<Pointer, std::weak_ptr>::value, void>::type
        print_internal(const Pointer& value, size_t indent = 0, const std::string& line_terminator = "\n",
        size_t level = 0) {
      stream_ << std::string(indent, ' ') << "<" <<
      type(value) << " at " << &value << ">"
                  << line_terminator;
    }

  };

}