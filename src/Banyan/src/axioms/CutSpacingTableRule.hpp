#pragma once

#include <assert.h>
#include <inttypes.h>

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

#include "axioms/CutClassRule.hpp"
#include "axioms/CutSpacingRule.hpp"
#include "axioms/Via.hpp"
#include "magic_enum.hpp"
#include "odb/db.h"
#include "utils.hpp"

namespace banyan {
class CutSpacingTableOrthogonal
{
 public:
  CutSpacingTableOrthogonal(){};
  ~CutSpacingTableOrthogonal(){};
  friend std::ostream& operator<<(std::ostream& out,
                                  const CutSpacingTableOrthogonal& obj)
  {
    out << " Orthogonal (Unsupport Rule)";
    return out;
  };
};

class CutSpacingTableItemSubPrl
{
 private:
  int32_t prl;
  bool maxxy;

 public:
  friend class CutSpacingTableRule;
  CutSpacingTableItemSubPrl(int32_t prl, bool maxxy) : prl(prl), maxxy(maxxy){};
  ~CutSpacingTableItemSubPrl(){};

  friend std::ostream& operator<<(std::ostream& out,
                                  const CutSpacingTableItemSubPrl& obj)
  {
    out << "\n\tPRL " << obj.prl;
    if (obj.maxxy) {
      out << " MAXXY ";
    }
    return out;
  };
};

enum class CutClassEdge
{
  SIDE,
  END
};

class CutSpacingTableItemSpacing
{
 private:
  uint32_t cutClass1Idx;
  std::optional<std::shared_ptr<CutClassRule>> cutClass1;
  std::optional<CutClassEdge> cutClass1Edge;
  uint32_t cutClass2Idx;
  std::optional<std::shared_ptr<CutClassRule>> cutClass2;
  std::optional<CutClassEdge> cutClass2Edge;
  uint32_t cutSpacing1;
  uint32_t cutSpacing2;

 public:
  CutSpacingTableItemSpacing(
      uint32_t cutClass1Idx,
      std::optional<std::shared_ptr<CutClassRule>> cutClass1,
      std::optional<CutClassEdge> cutClass1Edge,
      uint32_t cutClass2Idx,
      std::optional<std::shared_ptr<CutClassRule>> cutClass2,
      std::optional<CutClassEdge> cutClass2Edge,
      uint32_t cutSpacing1,
      uint32_t cutSpacing2)
      : cutClass1Idx(cutClass1Idx),
        cutClass1(cutClass1),
        cutClass1Edge(cutClass1Edge),
        cutClass2Idx(cutClass2Idx),
        cutClass2(cutClass2),
        cutClass2Edge(cutClass2Edge),
        cutSpacing1(cutSpacing1),
        cutSpacing2(cutSpacing2){};

  ~CutSpacingTableItemSpacing(){};

  uint32_t getCutClass1Idx() const { return cutClass1Idx; }

  std::optional<std::shared_ptr<CutClassRule>> getCutClass1() const
  {
    return cutClass1;
  }

  std::optional<CutClassEdge> getCutClass1Edge() const { return cutClass1Edge; }

  uint32_t getCutClass2Idx() const { return cutClass2Idx; }

  std::optional<std::shared_ptr<CutClassRule>> getCutClass2() const
  {
    return cutClass2;
  }

  std::optional<CutClassEdge> getCutClass2Edge() const { return cutClass2Edge; }

  uint32_t getCutSpacing1() const { return cutSpacing1; }

  uint32_t getCutSpacing2() const { return cutSpacing2; }

  friend std::ostream& operator<<(std::ostream& out,
                                  const CutSpacingTableItemSpacing& obj)
  {
    return out;
  };
};

class CutSpacingTableDefault
{
 private:
  std::optional<uint32_t> cutSpacing;
  std::optional<CutSpacingValidCondition> validCondition;
  std::optional<odb::dbTechLayer*> toLayer;
  std::optional<CutSpacingTableItemSubPrl> prl;
  std::vector<std::shared_ptr<CutSpacingTableItemSpacing>> items;

 public:
  CutSpacingTableDefault(
      std::optional<uint32_t> cutSpacing,
      std::optional<CutSpacingValidCondition> validCondition,
      std::optional<odb::dbTechLayer*> toLayer,
      std::optional<CutSpacingTableItemSubPrl> prl,
      std::vector<std::shared_ptr<CutSpacingTableItemSpacing>>&& items)
      : cutSpacing(cutSpacing),validCondition(validCondition), toLayer(toLayer), prl(prl), items(items){};

  ~CutSpacingTableDefault(){};

  bool isSameNetRule();

  friend class CutSpacingTableRule;
  friend std::ostream& operator<<(std::ostream& out,
                                  const CutSpacingTableDefault& obj)
  {
    if (obj.cutSpacing.has_value()) {
      out << "\n\tDEFAULT " << obj.cutSpacing.value();
    }
    if (obj.toLayer.has_value()) {
      out << "\n\tLAYER " << obj.toLayer.value()->getName();
    }
    if (obj.prl.has_value()) {
      out << obj.prl.value();
    }

    if (obj.items.size() > 0) {
      std::set<uint32_t> cutClass1Idxs;
      std::unordered_map<uint32_t,
                         std::pair<std::optional<std::shared_ptr<CutClassRule>>,
                                   std::optional<CutClassEdge>>>
          cutClass1Map;
      std::set<uint32_t> cutClass2Idxs;
      std::unordered_map<uint32_t,
                         std::pair<std::optional<std::shared_ptr<CutClassRule>>,
                                   std::optional<CutClassEdge>>>
          cutClass2Map;
      std::map<std::pair<uint32_t, uint32_t>, std::pair<uint32_t, uint32_t>>
          cutSapcingsMap;
      for (auto item : obj.items) {
        cutClass1Idxs.insert(item->getCutClass1Idx());
        cutClass1Map[item->getCutClass1Idx()]
            = std::make_pair(item->getCutClass1(), item->getCutClass1Edge());

        cutClass2Idxs.insert(item->getCutClass2Idx());
        cutClass2Map[item->getCutClass2Idx()]
            = std::make_pair(item->getCutClass2(), item->getCutClass2Edge());

        cutSapcingsMap[std::make_pair(item->getCutClass1Idx(),
                                      item->getCutClass2Idx())]
            = std::make_pair(item->getCutSpacing1(), item->getCutSpacing2());
      }

      out << "\n\tCUTCLASS ";
      for (auto cutClass1Idx : cutClass1Idxs) {
        auto [cutClass1, cutClass1Edge] = cutClass1Map[cutClass1Idx];
        out << " "
            << (cutClass1.has_value() ? cutClass1.value()->getClassName()
                                      : std::string("ALL"))
            << (cutClass1Edge.has_value() ? (
                    "/"
                    + std::string(magic_enum::enum_name(cutClass1Edge.value())))
                                          : "")
            << " ";
      }

      for (auto cutClass2Idx : cutClass2Idxs) {
        auto [cutClass2, cutClass2Edge] = cutClass2Map[cutClass2Idx];
        out << "\n"
            << (cutClass2.has_value() ? cutClass2.value()->getClassName()
                                      : std::string("ALL"))
            << (cutClass2Edge.has_value() ? (
                    "/"
                    + std::string(magic_enum::enum_name(cutClass2Edge.value())))
                                          : "")
            << " ";
        for (auto cutClass1Idx : cutClass1Idxs) {
          auto [cutSpacing1, cutSpacing2]
              = cutSapcingsMap[std::make_pair(cutClass1Idx, cutClass2Idx)];
          out << cutSpacing1 << " " << cutSpacing2 << " ";
        }
      }
      out << "\n";
    }
    return out;
  };
};

class CutSpacingTableRule
{
 private:
  odb::dbTechLayer* layer;
  std::
      variant<std::monostate, CutSpacingTableOrthogonal, CutSpacingTableDefault>
          subRule;

 public:
  CutSpacingTableRule(odb::dbTechLayer* layer,
                      odb::dbTechLayerCutSpacingTableDefRule* rule)
      : layer(layer)
  {
    std::vector<std::vector<std::pair<int, int>>> cutClassToCutClass;
    std::map<std::string, uint> rowMap;
    std::map<std::string, uint> colMap;
    rule->getSpacingTable(cutClassToCutClass, rowMap, colMap);

    auto parseTableTitleFn = [](std::string nameEdge)
        -> std::pair<std::string, std::optional<CutClassEdge>> {
      std::vector<std::string> rowStrs;
      boost::split(rowStrs, nameEdge, boost::is_any_of("/"));
      assert(rowStrs.size() > 0);
      auto name = rowStrs.at(0);
      auto edge = rowStrs.size() > 1
                      ? magic_enum::enum_cast<CutClassEdge>(rowStrs.at(1))
                      : std::nullopt;
      return std::make_pair(name, edge);
    };

    std::vector<std::shared_ptr<CutSpacingTableItemSpacing>> items;
    for (auto [rowKey, rowIdx] : rowMap) {
      auto [rowName, rowEdge] = parseTableTitleFn(rowKey);
      auto dbCutClass1 = CutClassRule::findCutClass(layer, rowName);
      auto cutClass1 = dbCutClass1.has_value()
                           ? std::make_optional(dbCutClass1.value())
                           : std::nullopt;
      for (auto [colKey, colIdx] : colMap) {
        auto [colName, colEdge] = parseTableTitleFn(colKey);
        auto dbCutClass2 = CutClassRule::findCutClass(layer, colName);
        auto cutClass2 = dbCutClass2.has_value()
                             ? std::make_optional(dbCutClass2.value())
                             : std::nullopt;

        auto spacings = cutClassToCutClass[rowIdx][colIdx];
        items.push_back(std::make_shared<CutSpacingTableItemSpacing>(
            CutSpacingTableItemSpacing(rowIdx,
                                       cutClass1,
                                       rowEdge,
                                       colIdx,
                                       cutClass2,
                                       colEdge,
                                       (uint32_t) spacings.first,
                                       (uint32_t) spacings.second)));
      }
    }

    auto vaildCondi
        = rule->isSameNet()
              ? std::make_optional(CutSpacingValidCondition::SAMENET)
          : rule->isSameMetal()
              ? std::make_optional(CutSpacingValidCondition::SAMEMETAL)
          : rule->isSameVia()
              ? std::make_optional(CutSpacingValidCondition::SAMEVIA)
              : std::nullopt;

    subRule = CutSpacingTableDefault(
        rule->isDefaultValid()
            ? std::make_optional((uint32_t) rule->getDefault())
            : std::nullopt,
        vaildCondi,
        rule->isLayerValid() ? std::make_optional(rule->getSecondLayer())
                             : std::nullopt,
        rule->isPrlValid() ? std::make_optional(
            CutSpacingTableItemSubPrl(rule->getPrl(), rule->isMaxXY()))
                           : std::nullopt,
        std::move(items));
  };

  ~CutSpacingTableRule(){};

  friend std::ostream& operator<<(std::ostream& out,
                                  const CutSpacingTableRule& obj)
  {
    out << obj.layer->getName() << "\tCUT SPACINGTABLE";

    auto srValue = obj.subRule;
    std::visit(
        overloaded{
            [](std::monostate&) {},
            [&out](CutSpacingTableOrthogonal& orth) { out << "\t" << orth; },
            [&out](CutSpacingTableDefault& def) { out << "\t" << def; },
        },
        srValue);
    return out;
  };

  std::vector<std::tuple<bool, std::optional<int64_t>, uint64_t>>
  getCutSpacings(std::shared_ptr<Via> via1,
                 std::shared_ptr<Via> via2,
                 bool isSameNet);
};

}  // namespace banyan