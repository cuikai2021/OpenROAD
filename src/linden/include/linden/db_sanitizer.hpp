#pragma once
#include <set>
#include <cstddef>

namespace odb {
class dbInst;
class dbDatabase;
} // namespace odb

namespace utl {
class Logger;
}

namespace linden {

class LindenDBSanitizer {
public:
  LindenDBSanitizer();
  ~LindenDBSanitizer();

  size_t removeFillers();

private:
  void initLogicalInstSet();
  bool isFillerInstByName(odb::dbInst* inst) const;
  bool isFillerInstByMasterType(odb::dbInst* inst) const;
  bool isUnconnected(odb::dbInst* inst) const;
  bool hasNoLogicPins(odb::dbInst* inst) const;
  bool isFiller(odb::dbInst *inst) const;

private:
  odb::dbDatabase* db_;
  utl::Logger* logger_;
  std::set<odb::dbInst*> logical_insts_;
};

} // namespace linden ends