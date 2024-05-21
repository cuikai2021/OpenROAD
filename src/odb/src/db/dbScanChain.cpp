#if 0
///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (c) 2022, The Regents of the University of California
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

// Generator Code Begin Cpp
#include "dbScanChain.h"

#include "dbBlock.h"
#include "dbDatabase.h"
#include "dbDft.h"
#include "dbDiff.hpp"
#include "dbScanInst.h"
#include "dbScanPartition.h"
#include "dbScanPin.h"
#include "dbTable.h"
#include "dbTable.hpp"
#include "odb/db.h"
#include "odb/dbSet.h"
namespace odb {
template class dbTable<_dbScanChain>;

bool _dbScanChain::operator==(const _dbScanChain& rhs) const
{
  if (name_ != rhs.name_) {
    return false;
  }
  if (scan_in_ != rhs.scan_in_) {
    return false;
  }
  if (scan_out_ != rhs.scan_out_) {
    return false;
  }
  if (scan_enable_ != rhs.scan_enable_) {
    return false;
  }
  if (test_mode_ != rhs.test_mode_) {
    return false;
  }
  if (test_mode_name_ != rhs.test_mode_name_) {
    return false;
  }
  if (*scan_partitions_ != *rhs.scan_partitions_) {
    return false;
  }

  return true;
}

bool _dbScanChain::operator<(const _dbScanChain& rhs) const
{
  return true;
}

void _dbScanChain::differences(dbDiff& diff,
                               const char* field,
                               const _dbScanChain& rhs) const
{
  DIFF_BEGIN
  DIFF_FIELD(name_);
  DIFF_FIELD(scan_in_);
  DIFF_FIELD(scan_out_);
  DIFF_FIELD(scan_enable_);
  DIFF_FIELD(test_mode_);
  DIFF_FIELD(test_mode_name_);
  DIFF_TABLE(scan_partitions_);
  DIFF_END
}

void _dbScanChain::out(dbDiff& diff, char side, const char* field) const
{
  DIFF_OUT_BEGIN
  DIFF_OUT_FIELD(name_);
  DIFF_OUT_FIELD(scan_in_);
  DIFF_OUT_FIELD(scan_out_);
  DIFF_OUT_FIELD(scan_enable_);
  DIFF_OUT_FIELD(test_mode_);
  DIFF_OUT_FIELD(test_mode_name_);
  DIFF_OUT_TABLE(scan_partitions_);

  DIFF_END
}

_dbScanChain::_dbScanChain(_dbDatabase* db)
{
  scan_partitions_ = new dbTable<_dbScanPartition>(
      db,
      this,
      (GetObjTbl_t) &_dbScanChain::getObjectTable,
      dbScanPartitionObj);
}

_dbScanChain::_dbScanChain(_dbDatabase* db, const _dbScanChain& r)
{
  name_ = r.name_;
  scan_in_ = r.scan_in_;
  scan_out_ = r.scan_out_;
  scan_enable_ = r.scan_enable_;
  test_mode_ = r.test_mode_;
  test_mode_name_ = r.test_mode_name_;
  scan_partitions_
      = new dbTable<_dbScanPartition>(db, this, *r.scan_partitions_);
}

dbIStream& operator>>(dbIStream& stream, _dbScanChain& obj)
{
  stream >> obj.name_;
  stream >> obj.scan_in_;
  stream >> obj.scan_out_;
  stream >> obj.scan_enable_;
  stream >> obj.test_mode_;
  stream >> obj.test_mode_name_;
  stream >> *obj.scan_partitions_;
  return stream;
}

dbOStream& operator<<(dbOStream& stream, const _dbScanChain& obj)
{
  stream << obj.name_;
  stream << obj.scan_in_;
  stream << obj.scan_out_;
  stream << obj.scan_enable_;
  stream << obj.test_mode_;
  stream << obj.test_mode_name_;
  stream << *obj.scan_partitions_;
  return stream;
}

dbObjectTable* _dbScanChain::getObjectTable(dbObjectType type)
{
  switch (type) {
    case dbScanPartitionObj:
      return scan_partitions_;
    default:
      break;
  }
  return getTable()->getObjectTable(type);
}

_dbScanChain::~_dbScanChain()
{
  delete scan_partitions_;
}

////////////////////////////////////////////////////////////////////
//
// dbScanChain - Methods
//
////////////////////////////////////////////////////////////////////

dbSet<dbScanPartition> dbScanChain::getScanPartitions() const
{
  _dbScanChain* obj = (_dbScanChain*) this;
  return dbSet<dbScanPartition>(obj, obj->scan_partitions_);
}

// User Code Begin dbScanChainPublicMethods

const std::string& dbScanChain::getName() const
{
  _dbScanChain* scan_chain = (_dbScanChain*) this;
  return scan_chain->name_;
}

void dbScanChain::setName(std::string_view name)
{
  _dbScanChain* scan_chain = (_dbScanChain*) this;
  scan_chain->name_ = name;
}

std::variant<dbBTerm*, dbITerm*> _dbScanChain::getPin(
    const dbId<dbScanPin>& scan_pin_id)
{
  _dbDft* dft = (_dbDft*) getOwner();
  return ((dbScanPin*) dft->scan_pins_->getPtr((dbId<_dbScanPin>) scan_pin_id))
      ->getPin();
}

void _dbScanChain::setPin(dbId<dbScanPin> _dbScanChain::*field, dbBTerm* pin)
{
  dbDft* dft = (dbDft*) getOwner();
  this->*field = dbScanPin::create(dft, pin);
}

void _dbScanChain::setPin(dbId<dbScanPin> _dbScanChain::*field, dbITerm* pin)
{
  dbDft* dft = (dbDft*) getOwner();
  this->*field = dbScanPin::create(dft, pin);
}

void dbScanChain::setScanIn(dbBTerm* scan_in)
{
  _dbScanChain* obj = (_dbScanChain*) this;
  obj->setPin(&_dbScanChain::scan_in_, scan_in);
}

void dbScanChain::setScanIn(dbITerm* scan_in)
{
  _dbScanChain* obj = (_dbScanChain*) this;
  obj->setPin(&_dbScanChain::scan_in_, scan_in);
}

std::variant<dbBTerm*, dbITerm*> dbScanChain::getScanIn() const
{
  _dbScanChain* obj = (_dbScanChain*) this;
  return obj->getPin(obj->scan_in_);
}

void dbScanChain::setScanOut(dbBTerm* scan_out)
{
  _dbScanChain* obj = (_dbScanChain*) this;
  obj->setPin(&_dbScanChain::scan_out_, scan_out);
}

void dbScanChain::setScanOut(dbITerm* scan_out)
{
  _dbScanChain* obj = (_dbScanChain*) this;
  obj->setPin(&_dbScanChain::scan_out_, scan_out);
}

std::variant<dbBTerm*, dbITerm*> dbScanChain::getScanOut() const
{
  _dbScanChain* obj = (_dbScanChain*) this;
  return obj->getPin(obj->scan_out_);
}

void dbScanChain::setScanEnable(dbBTerm* scan_enable)
{
  _dbScanChain* obj = (_dbScanChain*) this;
  obj->setPin(&_dbScanChain::scan_enable_, scan_enable);
}

void dbScanChain::setScanEnable(dbITerm* scan_enable)
{
  _dbScanChain* obj = (_dbScanChain*) this;
  obj->setPin(&_dbScanChain::scan_enable_, scan_enable);
}

std::variant<dbBTerm*, dbITerm*> dbScanChain::getScanEnable() const
{
  _dbScanChain* obj = (_dbScanChain*) this;
  return obj->getPin(obj->scan_enable_);
}

void dbScanChain::setTestMode(dbBTerm* test_mode)
{
  _dbScanChain* obj = (_dbScanChain*) this;
  obj->setPin(&_dbScanChain::test_mode_, test_mode);
}

void dbScanChain::setTestMode(dbITerm* test_mode)
{
  _dbScanChain* obj = (_dbScanChain*) this;
  obj->setPin(&_dbScanChain::test_mode_, test_mode);
}

std::variant<dbBTerm*, dbITerm*> dbScanChain::getTestMode() const
{
  _dbScanChain* obj = (_dbScanChain*) this;
  return obj->getPin(obj->test_mode_);
}

void dbScanChain::setTestModeName(const std::string& test_mode_name)
{
  _dbScanChain* scan_chain = (_dbScanChain*) this;
  scan_chain->test_mode_name_ = test_mode_name;
}

const std::string& dbScanChain::getTestModeName() const
{
  _dbScanChain* scan_chain = (_dbScanChain*) this;
  return scan_chain->test_mode_name_;
}

dbScanChain* dbScanChain::create(dbDft* dft)
{
  _dbDft* obj = (_dbDft*) dft;
  return (dbScanChain*) obj->scan_chains_->create();
}

// User Code End dbScanChainPublicMethods
}  // namespace odb
   // Generator Code End Cpp
#endif
#include "dbScanChain.h"

#include "db.h"
#include "dbTable.h"
#include "dbTable.hpp"
#include "dbDiff.h"
#include "dbDiff.hpp"
#include "dbBlock.h"
#include "dbITerm.h"
#include "dbBTerm.h"
#include "dbMTerm.h"
#include "dbInst.h"

namespace odb {

#define STREQ(X, Y) \
  ((X) == (Y) || ((X) && (Y) && strcmp((X), (Y)) == 0))

_dbScanChainComp::_dbScanChainComp(_dbDatabase* db) : _num_bits(1) {}

_dbScanChainComp::_dbScanChainComp(_dbDatabase* db, const _dbScanChainComp &rhs) :
  _comp(rhs._comp), _in(rhs._in), _out(rhs._out), _num_bits(rhs._num_bits) {
}

bool
_dbScanChainComp::operator==(const _dbScanChainComp& rhs) const {
  if (_comp != rhs._comp) return false;
  if (_in   != rhs._in)   return false;
  if (_out  != rhs._out)  return false;
  if (_num_bits != rhs._num_bits) return false;
  return true;
}

bool
_dbScanChainComp::operator< (const _dbScanChainComp& rhs) const {
  if (!(_num_bits < rhs._num_bits)) return false;
  return true;
}

void
_dbScanChainComp::differences(dbDiff& diff, const char* field, const _dbScanChainComp& rhs) const {
  DIFF_BEGIN
  DIFF_FIELD(_comp);
  DIFF_FIELD(_in);
  DIFF_FIELD(_out);
  DIFF_FIELD(_num_bits);
  DIFF_END
}

void
_dbScanChainComp::out(dbDiff& diff, char side, const char*field) const {
  DIFF_OUT_BEGIN
  DIFF_OUT_FIELD(_comp);
  DIFF_OUT_FIELD(_in);
  DIFF_OUT_FIELD(_out);
  DIFF_OUT_FIELD(_num_bits);
  DIFF_END
}

dbOStream&
operator<<(dbOStream& stream, const _dbScanChainComp& sc) {
  stream << sc._comp << sc._in << sc._out << sc._num_bits;
  return stream;
}

dbIStream&
operator>>(dbIStream& stream, _dbScanChainComp& sc) {
  stream >> sc._comp >> sc._in >> sc._out >> sc._num_bits;
  return stream;
}

_dbScanChainList::_dbScanChainList(_dbDatabase* db) :
  _list(new dbTable<_dbScanChainComp>(db, this, (GetObjTbl_t) &_dbScanChainList::getObjectTable, dbScanChainCompObj)) {
  ZALLOCATED(_list);
}

_dbScanChainList::_dbScanChainList(_dbDatabase* db, const _dbScanChainList & rhs) :
  _list(new dbTable<_dbScanChainComp>(db, this, *rhs._list)) {
	ZALLOCATED(_list);
}

_dbScanChainList::~_dbScanChainList() {
  delete _list;
}

bool
_dbScanChainList::operator==(const _dbScanChainList& rhs) const {
  return !(*_list != *rhs._list);
}

void
_dbScanChainList::differences(dbDiff& diff, const char* field, const _dbScanChainList& rhs) const {
  DIFF_BEGIN
  DIFF_TABLE(_list);
  DIFF_END
}

void
_dbScanChainList::out(dbDiff& diff, char side, const char* field) const {
  DIFF_OUT_BEGIN
  DIFF_OUT_TABLE(_list);
  DIFF_END
}

dbObjectTable*
_dbScanChainList::getObjectTable(dbObjectType type) {
  switch (type) {
  case dbScanChainCompObj:
	  return _list;
  default: break;
  }
  return getTable()->getObjectTable(type);
}

dbOStream&
operator<<(dbOStream& stream, const _dbScanChainList& sc) {
  stream << *sc._list;
  return stream;
}

dbIStream&
operator>>(dbIStream& stream, _dbScanChainList& sc) {
  stream >> *sc._list;
  return stream;
}

dbObjectTable*
_dbScanChain::getObjectTable(dbObjectType type) {
  switch (type) {
  case dbScanChainListObj:
	  return _ordered;
  case dbScanChainCompObj:
	  return _floating;
  default: break;
  }
  return getTable()->getObjectTable(type);
}

_dbScanChain::_dbScanChain(_dbDatabase* db) :
  _name(NULL), _common_scan_in(NULL), _common_scan_out(NULL),
  _floating(new dbTable<_dbScanChainComp>(db, this, (GetObjTbl_t) &_dbScanChain::getObjectTable, dbScanChainCompObj)),
  _ordered (new dbTable<_dbScanChainList>(db, this, (GetObjTbl_t) &_dbScanChain::getObjectTable, dbScanChainListObj)),
  _partition_name(NULL), _partition_max_bits(0) {
	ZALLOCATED(_floating);
	ZALLOCATED(_ordered);
}

_dbScanChain::_dbScanChain(_dbDatabase* db, const _dbScanChain& rhs)
 : _name(rhs._name ? strdup(rhs._name) : NULL),
   _common_scan_in(rhs._common_scan_in ? strdup(rhs._common_scan_in) : NULL),
	_common_scan_out(rhs._common_scan_out ? strdup(rhs._common_scan_out) : NULL),
	_start(rhs._start),
	_stop(rhs._stop),
    _blkstart(rhs._blkstart),
    _blkstop(rhs._blkstop),
	_floating(new dbTable<_dbScanChainComp>(db, this, *rhs._floating)),
	_ordered (new dbTable<_dbScanChainList>(db, this, *rhs._ordered)),
	_partition_name(rhs._partition_name ? strdup(rhs._partition_name) : NULL),
	_partition_max_bits(rhs._partition_max_bits) {
	ZALLOCATED(_floating);
	ZALLOCATED(_ordered);
}

_dbScanChain::~_dbScanChain() {
  if (_name)            free(_name);
  if (_common_scan_in)  free(_common_scan_in);
  if (_common_scan_out) free(_common_scan_out);
  if (_partition_name)  free(_partition_name);
  delete _floating;
  delete _ordered;
}

bool
_dbScanChain::operator==(const _dbScanChain& rhs) const {
  if (!STREQ(_name, rhs._name)) return false;
  if (!STREQ(_common_scan_in,  rhs._common_scan_in))  return false;
  if (!STREQ(_common_scan_out, rhs._common_scan_out)) return false;
  if (_start    != rhs._start)      return false;
  if (_stop     != rhs._stop)       return false;
  if (_blkstart != rhs._blkstart)   return false;
  if (_blkstop  != rhs._blkstop)    return false;
  if (*_floating != *rhs._floating) return false;
  if (*_ordered  != *rhs._ordered)  return false;
  if (!STREQ(_partition_name, rhs._partition_name))   return false;
  if (_partition_max_bits != rhs._partition_max_bits) return false;
  return true;
}

bool
_dbScanChain::operator< (const _dbScanChain& rhs) const {
  return strcmp(_name, rhs._name) < 0;
}

void
_dbScanChain::differences(dbDiff& diff, const char* field, const _dbScanChain& rhs) const {
  DIFF_BEGIN
  DIFF_FIELD(_name);
  DIFF_FIELD(_common_scan_in);
  DIFF_FIELD(_common_scan_out);
  DIFF_FIELD(_start);
  DIFF_FIELD(_stop);
  DIFF_FIELD(_blkstart);
  DIFF_FIELD(_blkstop);
  DIFF_TABLE(_floating);
  DIFF_TABLE(_ordered);
  DIFF_FIELD(_partition_name);
  DIFF_FIELD(_partition_max_bits);
  DIFF_END
}

void
_dbScanChain::out(dbDiff& diff, char side, const char* field) const {
  DIFF_OUT_BEGIN
  DIFF_OUT_FIELD(_name);
  DIFF_OUT_FIELD(_common_scan_in);
  DIFF_OUT_FIELD(_common_scan_out);
  DIFF_OUT_FIELD(_start);
  DIFF_OUT_FIELD(_stop);
  DIFF_OUT_FIELD(_blkstart);
  DIFF_OUT_FIELD(_blkstop);
  DIFF_OUT_TABLE(_floating);
  DIFF_OUT_TABLE(_ordered);
  DIFF_OUT_FIELD(_partition_name);
  DIFF_OUT_FIELD(_partition_max_bits);
  DIFF_END
}

dbOStream&
operator<<(dbOStream& stream, const _dbScanChain& sc) {
  stream << sc._name << sc._common_scan_in << sc._common_scan_out;
  stream << sc._start << sc._stop;
  stream << sc._blkstart << sc._blkstop;
  stream << *sc._floating << *sc._ordered;
  stream << sc._partition_name << sc._partition_max_bits;
  return stream;
}

dbIStream&
operator>>(dbIStream& stream, _dbScanChain& sc) {
  stream >> sc._name >> sc._common_scan_in >> sc._common_scan_out;
  stream >> sc._start >> sc._stop;
  stream >> sc._blkstart >> sc._blkstop;
  stream >> *sc._floating >> *sc._ordered;
  stream >> sc._partition_name >> sc._partition_max_bits;
  return stream;
}

dbScanChain*
dbScanChain::create(dbBlock* block, const char* name, const char* common_scan_in,
  const char* common_scan_out, const char* partition_name, unsigned partition_max_bits) {
  _dbBlock* _block = (_dbBlock*)block;
  _dbScanChain* ret = _block->_scan_chain_tbl->create();
  ret->_name           = name ? strdup(name) : NULL;
  ret->_common_scan_in  = common_scan_in  ? strdup(common_scan_in)  : NULL;
  ret->_common_scan_out = common_scan_out ? strdup(common_scan_out) : NULL;
  ret->_partition_name  = partition_name ? strdup(partition_name) : NULL;
  ret->_partition_max_bits = partition_max_bits;
  return (dbScanChain*)ret;
}

void
dbScanChain::destroy(dbScanChain* sc) {
  _dbBlock* block = (_dbBlock*)sc->getImpl()->getOwner();
  dbProperty::destroyProperties(sc);
  block->_scan_chain_tbl->destroy((_dbScanChain*) sc);
}

dbScanChain*
dbScanChain::getScanChain(dbBlock* block, uint dbid) {
  _dbBlock* blk = (_dbBlock*)block;
  return (dbScanChain*) blk->_scan_chain_tbl->getPtr(dbid);
}

dbScanChainComp*
dbScanChainComp::getScanChainComp(dbScanChain* scanchain, uint dbid) {
  _dbScanChain* sc = (_dbScanChain*)scanchain;
  return (dbScanChainComp*) sc->_floating->getPtr(dbid);
}

dbScanChainComp*
dbScanChainComp::getScanChainComp(dbScanChainList* scanchainlist, uint dbid) {
  _dbScanChainList* scl = (_dbScanChainList*)scanchainlist;
  return (dbScanChainComp*) scl->_list->getPtr(dbid);
}

void
dbScanChain::setStart(dbITerm* t) {
  _dbScanChain* sc = (_dbScanChain*)this;
  sc->_start = t->getId();
}

void
dbScanChain::setStop(dbITerm* t) {
  _dbScanChain* sc = (_dbScanChain*)this;
  sc->_stop = t->getId();
}

void
dbScanChain::setStart(dbBTerm* t) {
  _dbScanChain* sc = (_dbScanChain*)this;
  sc->_blkstart = t->getId();
}

void
dbScanChain::setStop(dbBTerm* t) {
  _dbScanChain* sc = (_dbScanChain*)this;
  sc->_blkstop = t->getId();
}

dbScanChainComp*
dbScanChainList::createComp(dbInst* inst, dbITerm* in, dbITerm* out, int num_bits) {
  _dbScanChainList* l = (_dbScanChainList*)this;
  _dbScanChainComp* comp = l->_list->create();
  comp->_comp = inst->getId();
  comp->_in   = in->getId();
  comp->_out  = out->getId();
  if (num_bits != -1)
    comp->_num_bits = num_bits;
  return (dbScanChainComp*)comp;
}

dbScanChainList*
dbScanChainList::getScanChainList(dbScanChain* scanchain, uint dbid) {
  _dbScanChain* sc = (_dbScanChain*)scanchain;
  return (dbScanChainList*) sc->_ordered->getPtr(dbid);
}

dbScanChainComp*
dbScanChain::createFloatingComp(dbInst* inst, dbITerm* in, dbITerm* out, int num_bits) {
  _dbScanChain* sc = (_dbScanChain*)this;
  _dbScanChainComp* comp = sc->_floating->create();
  comp->_comp = inst->getId();
  comp->_in   = in->getId();
  comp->_out  = out->getId();
  if (num_bits != -1)
    comp->_num_bits = num_bits;
  return (dbScanChainComp*)comp;
}

dbScanChainList*
dbScanChain::createOrderedList() {
  _dbScanChain* sc = (_dbScanChain*)this;
  _dbScanChainList* l = sc->_ordered->create();
  return (dbScanChainList*)l;
}

const char*
dbScanChain::getName() const {
  _dbScanChain* sc = (_dbScanChain*)this;
  return sc->_name;
}

dbITerm*
dbScanChain::getStart() const {
  _dbScanChain* sc = (_dbScanChain*)this;
  _dbBlock* blk = (_dbBlock*)sc->getOwner();
  return sc->_start.isValid() ? (dbITerm*)blk->_iterm_tbl->getPtr(sc->_start) : nullptr;
}

dbITerm*
dbScanChain::getStop() const {
  _dbScanChain* sc = (_dbScanChain*)this;
  _dbBlock* blk = (_dbBlock*)sc->getOwner();
  return sc->_stop.isValid() ? (dbITerm*)blk->_iterm_tbl->getPtr(sc->_stop) : nullptr;
}

dbBTerm*
dbScanChain::getBTermStart() const {
  _dbScanChain* sc = (_dbScanChain*)this;
  _dbBlock* blk = (_dbBlock*)sc->getOwner();
  return sc->_blkstart.isValid() ? (dbBTerm*)blk->_bterm_tbl->getPtr(sc->_blkstart) : nullptr;
}

dbBTerm*
dbScanChain::getBTermStop() const {
  _dbScanChain* sc = (_dbScanChain*)this;
  _dbBlock* blk = (_dbBlock*)sc->getOwner();
  return sc->_blkstop.isValid() ? (dbBTerm*)blk->_bterm_tbl->getPtr(sc->_blkstop) : nullptr;
}

const char*
dbScanChain::getPartitionName() const {
  _dbScanChain* sc = (_dbScanChain*)this;
  return sc->_partition_name;
}

uint
dbScanChain::getPartitionMaxBits() const {
  _dbScanChain* sc = (_dbScanChain*)this;
  return sc->_partition_max_bits;
}

dbSet<dbScanChainList>
dbScanChain::getOrdered() const {
  _dbScanChain* sc = (_dbScanChain*)this;
  return dbSet<dbScanChainList>(sc, sc->_ordered);
}

dbSet<dbScanChainComp>
dbScanChain::getFloating() const {
  _dbScanChain* sc = (_dbScanChain*)this;
  return dbSet<dbScanChainComp>(sc, sc->_floating);
}

dbSet<dbScanChainComp>
dbScanChainList::getComps() const {
  _dbScanChainList* l = (_dbScanChainList*)this;
  return dbSet<dbScanChainComp>(l, l->_list);
}

template< class T , dbObjectType type>
static T* get_owner(_dbObject* dbo) {
  if (!dbo) return nullptr;
  _dbObject* o = (_dbObject*)dbo;
  while (o->getType() != type)
	  o = (_dbObject*)o->getOwner();
  return (T*)o;
}

dbInst*
dbScanChainComp::getInst() const {
  _dbScanChainComp* l = (_dbScanChainComp*)this;
  _dbBlock* blk = get_owner<_dbBlock, dbBlockObj>(l);
  return (dbInst*)blk->_inst_tbl->getPtr(l->_comp);
}

dbITerm*
dbScanChainComp::getScanIn() const {
  _dbScanChainComp* l = (_dbScanChainComp*)this;
  _dbBlock* blk = get_owner<_dbBlock, dbBlockObj>(l);
  return (dbITerm*)blk->_iterm_tbl->getPtr(l->_in);
}

dbITerm*
dbScanChainComp::getScanOut() const {
  _dbScanChainComp* l = (_dbScanChainComp*)this;
  _dbBlock* blk = get_owner<_dbBlock, dbBlockObj>(l);
  return (dbITerm*)blk->_iterm_tbl->getPtr(l->_out);
}

uint
dbScanChainComp::getNumBits() const {
  _dbScanChainComp* l = (_dbScanChainComp*)this;
  return l->_num_bits;
}

} // end namespace odb
