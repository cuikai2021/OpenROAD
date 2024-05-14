#pragma once
#include <unordered_set>
#include <cstddef>

namespace odb {
class dbInst;
class dbDatabase;
} // namespace odb

namespace linden::remove_filler {

class LindenDBSanitizer {
public:
    LindenDBSanitizer();
    ~LindenDBSanitizer() =default;

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
    std::unordered_set<odb::dbInst*> logical_insts_;
};

} // namespace linden::remove_filler