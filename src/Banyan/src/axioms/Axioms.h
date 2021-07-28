#pragma once

#include <map>
#include <cstdint>


#include "DBAdapter.h"
#include "banyan_go.h"

namespace odb {
class dbDatabase;
class dbTechLayer;
class dbTechVia;
class dbBox;
}  // namespace odb

namespace utl {
class Logger;
}

namespace banyan {

enum POSITION{
    LEFT = 0,
    BOTTOM,
    RIGHT,
    TOP
};


class Axioms
{
 private:
  std::map<std::string, odb::dbTechVia*> defaultViaMap;
  std::vector<banyan_go::axioms_triple> axiomsTriples;
  std::vector<banyan_go::trunk_min_len_table> layerToTrunkMinLengthTable;
  std::vector<banyan_go::trunk_min_len_table> layerToTrunkDistSameTable;
  std::vector<banyan_go::trunk_min_len_table> layerToTrunkDistDiffTable;

 protected:
  OpenDBAdapter& dbAdapter_;
  odb::dbDatabase* db_;
  utl::Logger* logger_;
  void initDefaultViaMap();
  void initLayerToTrunkMinLengthTable();
  uint64_t calcTrunkMinLength(odb::dbTechLayer* layer,
                              odb::dbTechVia* viaX,
                              odb::dbTechVia* viaY);

  odb::dbBox* getLayerBoxFromVia(odb::dbTechLayer* layer, odb::dbTechVia* via);
  odb::dbBox* getCutBoxInBorderFromVia(odb::dbTechVia* via, POSITION position);
  uint32_t getCutNumsFromVia(odb::dbTechVia* via);
  uint64_t getCutSpacingForTrunkMinLengthFromVia(odb::dbTechVia* viaX, odb::dbTechVia* viaY);

  odb::dbTechLayer* getCutLayerFromVia(odb::dbTechVia* via);

  void initLayerToTrunkDistSameTable();
  void initLayerToTrunkDistDiffTable();

 public:
 const std::string VIA_NULL = "null";
  Axioms(OpenDBAdapter& dbAdapter) : dbAdapter_(dbAdapter), db_(dbAdapter.db_), logger_(dbAdapter.logger_)
  {
    initDefaultViaMap();
    initLayerToTrunkMinLengthTable();
    initLayerToTrunkDistSameTable();
    initLayerToTrunkDistDiffTable();
  };
  ~Axioms(){

  };

  std::vector<banyan_go::trunk_min_len_table> getLayerToTrunkMinLengthTable()
  {
    return layerToTrunkMinLengthTable;
  };
  std::vector<banyan_go::axioms_triple> getAxiomsTriples()
  {
    return axiomsTriples;
  }
};

}  // namespace banyan
