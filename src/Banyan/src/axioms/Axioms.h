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
class Rect;
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

class Axioms;
typedef uint64_t (Axioms::*CalcLength)(odb::dbTechLayer* layer,
                            odb::dbTechVia* viaX,
                            odb::dbTechVia* viaY,
                            bool isSameNet);

class Axioms
{
 private:
  std::map<std::string, odb::dbTechVia*> defaultViaMap;
  std::vector<banyan_go::axioms_triple> axiomsTriples;
  std::vector<banyan_go::axioms_layer_table> layerToTrunkMinLengthTable;
  std::vector<banyan_go::axioms_layer_table> layerToTrunkDistSameTable;
  std::vector<banyan_go::axioms_layer_table> layerToTrunkDistDiffTable;

 protected:
  OpenDBAdapter& dbAdapter_;
  odb::dbDatabase* db_;
  utl::Logger* logger_;

  void initDefaultViaMap();

  void initAxiomsTables();

  void getDefaultViasInLayer(odb::dbTechLayer* layer,std::vector<std::pair<std::string, odb::dbTechVia*>>& defaultViasInLayer);

  void addAxiomsTriples(odb::dbTechLayer* layer,std::vector<std::pair<std::string, odb::dbTechVia*>> defaultViasInLayer,CalcLength calcFunc,bool isSameNet);



  uint64_t calcTrunkMinLength(odb::dbTechLayer* layer,
                              odb::dbTechVia* viaX,
                              odb::dbTechVia* viaY,
                              bool isSameNet);

  odb::dbBox* getLayerBoxFromVia(odb::dbTechLayer* layer, odb::dbTechVia* via);

  void getBoundaryBoxOfCutBoxesFromVia(odb::dbTechVia* via, odb::Rect* resultBox);

  uint32_t getCutNumsFromVia(odb::dbTechVia* via);

  uint64_t getCutSpacingFromVia(odb::dbTechVia* viaX, odb::dbTechVia* viaY, bool isSameNet);

  uint64_t getRoutingSpacing(odb::dbTechLayer* layer, bool isSameNet);

  odb::dbTechLayer* getCutLayerFromVia(odb::dbTechVia* via);


  uint64_t calcTrunkDistLength(odb::dbTechLayer* layer,
                              odb::dbTechVia* viaX,
                              odb::dbTechVia* viaY,
                              bool isSameNet);

 public:
 const std::string VIA_NULL = "null";
  Axioms(OpenDBAdapter& dbAdapter) : dbAdapter_(dbAdapter), db_(dbAdapter.db_), logger_(dbAdapter.logger_)
  {
    initDefaultViaMap();
    initAxiomsTables();
  };
  ~Axioms(){

  };
  std::vector<banyan_go::axioms_layer_table> getLayerToTrunkMinLengthTable()
  {
    return layerToTrunkMinLengthTable;
  };
  std::vector<banyan_go::axioms_layer_table> getLayerToTrunkDistSameTable()
  {
    return layerToTrunkDistSameTable;
  };
  std::vector<banyan_go::axioms_layer_table> getLayerToTrunkDistDiffTable()
  {
    return layerToTrunkDistDiffTable;
  };
  std::vector<banyan_go::axioms_triple> getAxiomsTriples()
  {
    return axiomsTriples;
  }
};

}  // namespace banyan
