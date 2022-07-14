#pragma once

#include <functional>
#include <map>

#include "DBAdapter.hpp"
#include "Global.hpp"
#include "axioms/CutClassRule.hpp"
#include "axioms/CutSpacingRule.hpp"
#include "axioms/CutSpacingTableRule.hpp"
#include "axioms/Geometry.hpp"
#include "axioms/RoutingEOLSpacingRule.hpp"
#include "axioms/RoutingSpacingRule.hpp"
#include "axioms/RoutingSpacingTableRule.hpp"
#include "axioms/Via.hpp"
#include "banyan_go.h"
#include "odb/db.h"

#define CALL_MEMBER_FN(object, ptrToMember) ((object).*(ptrToMember))
#define CAST_AXIOMS_CALC_FN(ptrToMember)          \
  (uint64_t(Axioms::*)(odb::dbTechLayer * layer,  \
                       std::shared_ptr<Via> viaX, \
                       std::shared_ptr<Via> viaY, \
                       bool isSameNet))(ptrToMember)

#define DEBUG_TRUNKMINLENGTH 0
#define DEBUG_TRUNKDISTLENGTH 0
#define DEBUG_VIAVIATRUNKLENGTH 0
#define DEBUG_VIAVIATRACKLENGTH 0
#define DEBUG_VIATRACKLENGTH 0
#define DEBUG_SPACINGTABLE 0

namespace banyan {
template <typename T>
using LayerRules = std::vector<std::shared_ptr<T>>;
template <typename T>
using LayerRulesMap = std::map<odb::dbTechLayer*, LayerRules<T>>;

const int NULL_VIA_IDX = -1;

class OpenDBAdapter;

enum class AxiomsRuleType
{
  TrunkMinLength,
  TrunkDistLength,
  ViaViaTrunkLength,
  ViaViaTrackLength,
  ViaTrackLength
};
class Axioms
{
 public:
  uint64_t calcTrunkMinLength(odb::dbTechLayer* layer,
                              std::shared_ptr<Via> viaX,
                              std::shared_ptr<Via> viaY,
                              bool isSameNet);

  uint64_t calcTrunkDistLength(odb::dbTechLayer* layer,
                               std::shared_ptr<Via> viaX,
                               std::shared_ptr<Via> viaY,
                               bool isSameNet);

  uint64_t calcViaViaTrunkLength(odb::dbTechLayer* layer,
                                 std::shared_ptr<Via> viaX,
                                 std::shared_ptr<Via> viaY,
                                 bool isSameNet);

  uint64_t calcViaViaTrackLength(odb::dbTechLayer* layer,
                                 std::shared_ptr<Via> viaX,
                                 std::shared_ptr<Via> viaY,
                                 bool isSameNet);

  uint64_t calcViaTrackLength(odb::dbTechLayer* layer,
                              std::shared_ptr<Via> viaX,
                              std::shared_ptr<Via> viaY,
                              bool isSameNet);

 private:
  typedef uint64_t (Axioms::*CalcLengthFn)(odb::dbTechLayer* layer,
                                           std::shared_ptr<Via> viaX,
                                           std::shared_ptr<Via> viaY,
                                           bool isSameNet);
  std::unordered_map<AxiomsRuleType, CalcLengthFn> calcFnMap
      = {{AxiomsRuleType::TrunkMinLength,
          CAST_AXIOMS_CALC_FN(&Axioms::calcTrunkMinLength)},
         {AxiomsRuleType::TrunkDistLength,
          CAST_AXIOMS_CALC_FN(&Axioms::calcTrunkDistLength)},
         {AxiomsRuleType::ViaViaTrunkLength,
          CAST_AXIOMS_CALC_FN(&Axioms::calcViaViaTrunkLength)},
         {AxiomsRuleType::ViaViaTrackLength,
          CAST_AXIOMS_CALC_FN(&Axioms::calcViaViaTrackLength)},
         {AxiomsRuleType::ViaTrackLength,
          CAST_AXIOMS_CALC_FN(&Axioms::calcViaTrackLength)}};

  std::map<std::string, std::shared_ptr<Via>> viaMap;
  LayerRulesMap<CutSpacingRule> laToCutSpacingRules;
  LayerRulesMap<CutSpacingTableRule> laToCutSpacingTableRules;
  LayerRulesMap<RoutingEOLSpacingRule> laToRoutingEOLSpacingRules;
  LayerRulesMap<RoutingSpacingRule> laToRoutingSpacingRules;
  LayerRulesMap<RoutingSpacingTableRule> laToRoutingSpacingTableRules;

  bool debugActive_;

 protected:
  OpenDBAdapter& dbAdapter_;
  odb::dbDatabase* db_;
  utl::Logger* logger_;

  void initViaMap();

  void getViasInLayer(
      odb::dbTechLayer* layer,
      std::vector<std::pair<std::string, std::shared_ptr<Via>>>& viasInLayer);

  void initCutClasses();

  void initRules();

  void calcAxiomsRules();

  void calcSpacingTable();

  void calcMinimumCut();

  void calcDefaultSpacing();

  uint64_t getMinSpacing(odb::dbTechLayer* layer);

  uint64_t getMaxRoutingPrlSpacing(odb::dbTechLayer* layer, bool isSameNet);

  uint64_t getMaxRoutingEOLPrlSpacing(odb::dbTechLayer* layer,
                                      std::shared_ptr<Via> via,
                                      bool isSameNet);

  uint64_t getMaxRoutingEOLEndToEndSpacing(odb::dbTechLayer* layer,
                                           std::shared_ptr<Via> via,
                                           bool isSameNet);

  uint64_t getMaxCutPrlCenterToCenterSpacing(odb::dbTechLayer* layer,
                                             std::shared_ptr<Via> via1,
                                             std::shared_ptr<Via> via2,
                                             bool isSameNet);

  std::optional<uint32_t> findLengthThreshold(odb::dbTechLayer* layer);
  // return isCenterToCenter, prl, spacing
  std::vector<std::tuple<bool, std::optional<int64_t>, uint64_t>>
  getCutSpacings(std::shared_ptr<Via> via1,
                 std::shared_ptr<Via> via2,
                 bool isSameNet);

  std::optional<std::pair<Rectangle, Rectangle>> getConstrainedMetalBoxes(
      odb::dbTechLayer* layer,
      std::shared_ptr<Via> via1,
      std::shared_ptr<Via> via2);

  std::pair<uint64_t, uint64_t> addTableTriples(
      odb::dbTechLayer* layer,
      std::vector<std::pair<std::string, std::shared_ptr<Via>>> viasInLayer,
      CalcLengthFn calcFunc,
      bool isSameNet,
      std::function<bool(int, int)> viaPairFilter
      = [](int viaXId, int viaYId) -> bool { return true; });

 public:
  Axioms(OpenDBAdapter& dbAdapter, bool debugActive = false)
      : dbAdapter_(dbAdapter),
        db_(dbAdapter.db_),
        logger_(dbAdapter.logger_),
        debugActive_(debugActive)
  {
    initViaMap();
    initCutClasses();
    initRules();
    calcAxiomsRules();
    calcSpacingTable();
    calcMinimumCut();
    calcDefaultSpacing();
  };
  ~Axioms(){};
};

}  // namespace banyan
