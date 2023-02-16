#include "dbScanChain.h"

#include "ZComponents.h"
#include "db.h"
#include "dbTable.h"
#include "dbTable.hpp"
#include "dbDiff.h"
#include "dbDiff.hpp"
#include "dbBlock.h"
#include "dbITerm.h"
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
  stream << *sc._floating << *sc._ordered;
  stream << sc._partition_name << sc._partition_max_bits;
  return stream;
}

dbIStream&
operator>>(dbIStream& stream, _dbScanChain& sc) {
  stream >> sc._name >> sc._common_scan_in >> sc._common_scan_out;
  stream >> sc._start >> sc._stop;
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
  return (dbITerm*)blk->_iterm_tbl->getPtr(sc->_start);
}

dbITerm*
dbScanChain::getStop() const {
  _dbScanChain* sc = (_dbScanChain*)this;
  _dbBlock* blk = (_dbBlock*)sc->getOwner();
  return (dbITerm*)blk->_iterm_tbl->getPtr(sc->_stop);
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
