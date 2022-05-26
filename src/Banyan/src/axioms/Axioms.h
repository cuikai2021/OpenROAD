#pragma once

#include <cstdint>
#include <map>

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

const int NULL_VIA_IDX = -1;

enum POSITION
{
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

 protected:
  OpenDBAdapter& dbAdapter_;
  odb::dbDatabase* db_;
  utl::Logger* logger_;

  void initDefaultViaMap();

  void initTable();

  void getDefaultViasInLayer(
      odb::dbTechLayer* layer,
      std::vector<std::pair<std::string, odb::dbTechVia*>>& defaultViasInLayer);

  void addTableTriples(
      odb::dbTechLayer* layer,
      std::vector<std::pair<std::string, odb::dbTechVia*>> defaultViasInLayer,
      CalcLength calcFunc,
      bool isSameNet,
      bool excludedNullVia = false);

  odb::dbBox* getLayerBoxFromVia(odb::dbTechLayer* layer, odb::dbTechVia* via);

  void getBoundaryBoxOfCutBoxesFromVia(odb::dbTechVia* via,
                                       odb::Rect* resultBox);

  uint32_t getCutNumsFromVia(odb::dbTechVia* via);

  uint64_t getCutSpacingFromVia(odb::dbTechVia* viaX,
                                odb::dbTechVia* viaY,
                                bool isSameNet, bool isPrl);

  uint64_t getRoutingEOLSpacing(odb::dbTechLayer* layer, bool isSameNet);

  uint64_t getRoutingPRLSpacing(odb::dbTechLayer* layer, bool isSameNet);

  odb::dbTechLayer* getCutLayerFromVia(odb::dbTechVia* via);

  uint64_t getDiffAreaBetweenViaAndWire(odb::dbBox* metalBox, uint64_t width, bool isHorizontal);

  uint64_t getMarginBetweenCutAndMetalBox(odb::dbBox* metalBox, odb::Rect* cutBox, POSITION position);

  uint64_t getDiffAreaFromVia(odb::dbTechVia* via, uint64_t width, bool isHorizontal);


  uint64_t calcTrunkMinLength(odb::dbTechLayer* layer,
                              odb::dbTechVia* viaX,
                              odb::dbTechVia* viaY,
                              bool isSameNet);

  uint64_t calcTrunkDistLength(odb::dbTechLayer* layer,
                               odb::dbTechVia* viaX,
                               odb::dbTechVia* viaY,
                               bool isSameNet);

  uint64_t calcViaViaTrunkLength(odb::dbTechLayer* layer,
                              odb::dbTechVia* viaX,
                              odb::dbTechVia* viaY,
                              bool isSameNet);

  uint64_t calcViaViaTrackLength(odb::dbTechLayer* layer,
                              odb::dbTechVia* viaX,
                              odb::dbTechVia* viaY,
                              bool isSameNet);

  uint64_t calcViaTrackLength(odb::dbTechLayer* layer, odb::dbTechVia* via);

 public:
  const std::string VIA_NULL = "null";

  Axioms(OpenDBAdapter& dbAdapter)
      : dbAdapter_(dbAdapter), db_(dbAdapter.db_), logger_(dbAdapter.logger_)
  {
    initDefaultViaMap();
    initTable();
  };

  ~Axioms(){

  };

};

}  // namespace banyan
