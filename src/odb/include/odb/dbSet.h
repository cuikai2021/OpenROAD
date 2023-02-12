///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (c) 2019, Nefelus Inc
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "dbIterator.h"

namespace odb {

template <class T>
class dbSet;

template <class T>
class dbSetFilter
{
public:
  dbSetFilter() : _end(0), _refcnt(1) {}
  virtual ~dbSetFilter() {}
  virtual bool valid(T*) const { return true; }
  uint _refcnt;
  uint _end;
};

template <class T>
class dbSetIterator
{
  typedef dbSetFilter<T> dbFilter;
  friend class dbSet<T>;

  dbIterator*  _itr;
  uint         _cur;
  dbFilter*    _filter;

  T* getCur() { return (T*) _itr->getObject(_cur); }
  dbSetIterator(dbIterator* itr, uint id, dbFilter* f)
   : _itr(itr), _cur(id), _filter(f) { validOrNext(); }
  void validOrNext() {
	 if (!_filter) return;
	 do {
		if (_cur == _filter->_end) return;
		if (_filter->valid(getCur())) return;
		_cur = _itr->next(_cur);
	 } while(1);
  }
  void next() { _cur = _itr->next(_cur); validOrNext(); }
 public:
  typedef T* value_type;
  typedef std::ptrdiff_t difference_type;
  typedef T** pointer;
  typedef T*& reference;
  typedef std::input_iterator_tag iterator_category;

  dbSetIterator() : _itr(NULL), _cur(0), _filter(NULL) {}
  dbSetIterator(const dbSetIterator& it) = default;

  bool operator==(const dbSetIterator<T>& it) const { return (_itr == it._itr) && (_cur == it._cur); }
  bool operator!=(const dbSetIterator<T>& it) const { return !(*this == it); }

  T* operator*()  { return getCur(); }
  T* operator->() { return getCur(); }
  dbSetIterator<T>& operator++() { next(); return *this; }
  dbSetIterator<T> operator++(int) { dbSetIterator it(*this); ++(*this); return it; }
};

///
/// The dbSet class provides an abstration of database objects collections.
///
/// There are two types of sets; sequential and non-sequential.
///
/// Sequential set iterators have the following property:
///
///     Let (o1,o2,o3,...,oN) be the objects iterated of a set, where 1..N
///     represents the iteration order.
///
///     Then the following condition holds:
///
///        0 < o1->getId() < o2->getId() < o3->getId() < ... < oN->getId()
///
///     Because of object deletion, there may be gaps between the object
///     identifiers in the iterated set:
///
///        Given: ... < oi->getId() < oj->getId() < ...
///        Then: ... < (oi->getId() + 1) <= oj->getId() < ...
///
/// Non-Sequential set iterators do not have any ordering property.
///
template <class T>
class dbSet
{
  typedef dbSetFilter<T> dbFilter;
  dbIterator* _itr;
  dbObject*   _parent;
  dbFilter*   _filter;
 public:
  typedef dbSetIterator<T> iterator;

  dbSet() : _itr(NULL), _parent(NULL), _filter(NULL) {}
  dbSet(dbObject* parent, dbIterator* itr) : _itr(itr), _parent(parent), _filter(NULL) {}
  dbSet(const dbSet<T>& c) : _itr(c._itr), _parent(c._parent), _filter(c._filter) { if(_filter) ++_filter->_refcnt; }
  ~dbSet() { if (_filter && --_filter->_refcnt == 0) delete _filter; }

  ///
  /// Set iteration filter
  ///
  void set(dbFilter* f) {
	  assert(!_filter);
	  _filter = f;
	  _filter->_end = _itr->end(_parent);
  }

  ///
  /// Returns the number of items in this set.
  ///
  uint size() { return _itr->size(_parent); }

  ///
  /// Return a begin() iterator.
  ///
  iterator begin() const { return iterator(_itr, _itr->begin(_parent), _filter); }

  ///
  /// Return an end() iterator.
  ///
  iterator end() const { return iterator(_itr, _itr->end(_parent), _filter); }

  ///
  /// If this set is sequential, this function will return the database
  /// identifier of the "Nth" iterated element (see the class description
  /// above):
  ///
  ///      returns oN->getId()
  ///
  /// If this set is non-sequential then it returns 0.
  ///
  uint sequential() { return _itr->sequential(); }

  ///
  /// Returns true if this set is reversible.
  ///
  bool reversible() { return _itr->reversible(); }

  ///
  /// Returns true if the is iterated in the reverse order that
  /// it was created.
  ///
  bool orderReversed() { return _itr->orderReversed(); }

  ///
  /// Reverse the order of this set.
  ///
  void reverse() { _itr->reverse(_parent); }

  ///
  /// Returns true if set is empty
  ///
  bool empty() { return begin() == end(); }
};

}  // namespace odb

// Specialization definitions
#include "dbBlockSet.h"
#include "dbCCSegSet.h"
#include "dbNetSet.h"
