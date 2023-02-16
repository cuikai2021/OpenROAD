#pragma once

#include "dbCore.h"
#include "dbId.h"
#include "dbTypes.h"
#include "odb.h"

namespace odb {

template< class T> class dbTable;
class _dbInst;
class _dbITerm;

class _dbScanChainBase : public _dbObject {
public:
  typedef uint     Bits;
  typedef _dbITerm _dbTerm;
};

class _dbScanChainComp : public _dbScanChainBase {
public:
  dbId<_dbInst> _comp;
  dbId<_dbTerm> _in;
  dbId<_dbTerm> _out;
  Bits          _num_bits;

  _dbScanChainComp(_dbDatabase*);
  _dbScanChainComp(_dbDatabase*, const _dbScanChainComp & rhs);
  bool operator==(const _dbScanChainComp& rhs) const;
  bool operator!=(const _dbScanChainComp& rhs) const { return !operator==(rhs); }
  bool operator< (const _dbScanChainComp& rhs) const;
  void differences(dbDiff& diff, const char* field, const _dbScanChainComp& rhs) const;
  void out(dbDiff& diff, char side, const char* field) const;
};
dbOStream& operator<<(dbOStream& stream, const _dbScanChainComp& sc);
dbIStream& operator>>(dbIStream& stream, _dbScanChainComp& sc);

class _dbScanChainList : public _dbScanChainBase {
public:
  dbTable<_dbScanChainComp>* _list;

  _dbScanChainList(_dbDatabase*);
  _dbScanChainList(_dbDatabase*, const _dbScanChainList& s);
  ~_dbScanChainList();
  bool operator==(const _dbScanChainList& rhs) const;
  bool operator!=(const _dbScanChainList& rhs) const { return !operator==(rhs); }
  bool operator< (const _dbScanChainList& rhs) const { return true; }
  void differences(dbDiff& diff, const char* field, const _dbScanChainList& rhs) const;
  void out(dbDiff& diff, char side, const char* field) const;
  dbObjectTable* getObjectTable(dbObjectType type);
};
dbOStream& operator<<(dbOStream& stream, const _dbScanChainList& sc);
dbIStream& operator>>(dbIStream& stream, _dbScanChainList& sc);

class _dbScanChain : public _dbScanChainBase {
public:
  char*                      _name;
  char*                      _common_scan_in;
  char*                      _common_scan_out;
  dbId<_dbTerm>              _start;
  dbId<_dbTerm>              _stop;
  dbTable<_dbScanChainComp>* _floating;
  dbTable<_dbScanChainList>* _ordered;
  char*                      _partition_name;
  Bits                       _partition_max_bits;

  _dbScanChain(_dbDatabase*);
  _dbScanChain(_dbDatabase*, const _dbScanChain& s);
  ~_dbScanChain();
  bool operator==(const _dbScanChain& rhs) const;
  bool operator!=(const _dbScanChain& rhs) const { return !operator==(rhs); }
  bool operator< (const _dbScanChain& rhs) const;
  void differences(dbDiff& diff, const char* field, const _dbScanChain& rhs) const;
  void out(dbDiff& diff, char side, const char* field) const;
  dbObjectTable* getObjectTable(dbObjectType type);
};
dbOStream& operator<<(dbOStream& stream, const _dbScanChain& sc);
dbIStream& operator>>(dbIStream& stream, _dbScanChain& sc);

} // end namespace odb
