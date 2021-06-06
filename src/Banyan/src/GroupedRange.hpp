#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace utl {
template <typename Iterator, typename KeyFunc>
struct GroupBy
{
  typedef typename KeyFunc::value_type KeyValue;

  struct Range
  {
    Range(Iterator begin, Iterator end) : iter_pair(begin, end) {}

    Iterator begin() const { return iter_pair.first; }
    Iterator end() const { return iter_pair.second; }

   private:
    std::pair<Iterator, Iterator> iter_pair;
  };

  struct Group
  {
    KeyValue value;
    Range range;

    Group(KeyValue value, Range range) : value(value), range(range) {}
  };

  struct GroupIterator
  {
    typedef Group value_type;

    GroupIterator(Iterator iter, Iterator end, KeyFunc key_func)
        : range_begin(iter), range_end(iter), end(end), key_func(key_func)
    {
      advance_range_end();
    }

    bool operator==(const GroupIterator& that) const
    {
      return range_begin == that.range_begin;
    }

    bool operator!=(const GroupIterator& that) const
    {
      return !(*this == that);
    }

    GroupIterator operator++()
    {
      range_begin = range_end;
      advance_range_end();
      return *this;
    }

    value_type operator*()
    {
      return value_type(key_func(*range_begin), Range(range_begin, range_end));
    }

   private:
    void advance_range_end()
    {
      if (range_end != end) {
        typename KeyFunc::value_type value = key_func(*range_end++);
        while (range_end != end && key_func(*range_end) == value) {
          ++range_end;
        }
      }
    }

    Iterator range_begin;
    Iterator range_end;
    Iterator end;
    KeyFunc key_func;
  };

  GroupBy(Iterator begin_iter, Iterator end_iter, KeyFunc key_func)
      : begin_iter(begin_iter), end_iter(end_iter), key_func(key_func)
  {
  }

  GroupIterator begin()
  {
    return GroupIterator(begin_iter, end_iter, key_func);
  }

  GroupIterator end() { return GroupIterator(end_iter, end_iter, key_func); }

 private:
  Iterator begin_iter;
  Iterator end_iter;
  KeyFunc key_func;
};

template <typename Iterator, typename KeyFunc>
inline GroupBy<Iterator, KeyFunc> group_by(Iterator begin,
                                           Iterator end,
                                           const KeyFunc& key_func = KeyFunc())
{
  return GroupBy<Iterator, KeyFunc>(begin, end, key_func);
}
}  // namespace utl