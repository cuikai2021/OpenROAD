#pragma once

#include <assert.h>
#include <inttypes.h>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <functional>
#include <iostream>
#include <optional>
#include <set>
#include <utility>
#include <variant>

#include "odb/db.h"
#include "utils.hpp"

namespace banyan {

class RoutingSpacingTableRulePrl
{
 private:
  std::map<std::pair<uint32_t, uint32_t>, uint32_t> widthPrlToSpacings;

 public:
  RoutingSpacingTableRulePrl(
      std::map<std::pair<uint32_t, uint32_t>, uint32_t>&& widthPrlToSpacings)
      : widthPrlToSpacings(widthPrlToSpacings){};

  ~RoutingSpacingTableRulePrl(){};

  friend class RoutingSpacingTableRule;
  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingSpacingTableRulePrl& obj)
  {
    std::set<uint32_t> widths;
    std::set<uint32_t> prls;
    for (auto [pair, spacing] : obj.widthPrlToSpacings) {
      widths.insert(pair.first);
      prls.insert(pair.second);
    }
    out << "\tPARALLELRUNLENGTH";
    for (auto prl : prls) {
      out << " " << prl;
    }
    out << "\n";

    for (auto width : widths) {
      out << "\t\t\tWIDTH " << width;
      for (auto prl : prls) {
        auto spacing = obj.widthPrlToSpacings.at(std::make_pair(width, prl));
        out << " " << spacing;
      }
      out << "\n";
    }

    return out;
  };
};

class RoutingSpacingTableRuleTwoWidth
{
 public:
  friend class RoutingSpacingTableRule;
  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingSpacingTableRuleTwoWidth& obj)
  {
    out << "\t\tTwoWidth (Unsupport Rule)";
    return out;
  };
};

class RoutingSpacingTableRule
{
 private:
  odb::dbTechLayer* layer;
  std::set<uint32_t> widths;
  std::set<uint32_t> prls;
  std::variant<std::monostate,
               RoutingSpacingTableRulePrl,
               RoutingSpacingTableRuleTwoWidth>
      subRule;

 public:
  RoutingSpacingTableRule(odb::dbTechLayer* layer,
                          std::vector<uint>& widthIdx,
                          std::vector<uint>& lengthIdx)
      : layer(layer)
  {
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> widthPrlToSpacings;
    for (auto width : widthIdx) {
      for (auto prl : lengthIdx) {
        auto spacing = layer->findV55Spacing(width+1, prl+1);
        widthPrlToSpacings[std::make_pair((uint32_t) width, (uint32_t) prl)]
            = (uint32_t) spacing;
      }
    }
    for (auto [pair, spacing] : widthPrlToSpacings) {
      widths.insert(pair.first);
      prls.insert(pair.second);
    }
    subRule = RoutingSpacingTableRulePrl(std::move(widthPrlToSpacings));
  };

  RoutingSpacingTableRule(odb::dbTechLayer* layer,
                          odb::dbTechLayerSpacingTablePrlRule* rule)
      : layer(layer)
  {
    assert(!rule->isExceeptEol());
    assert(!rule->isSameMask());
    assert(!rule->isWrongDirection());
    std::vector<int> widthIdx;
    std::vector<int> lengthIdx;
    std::vector<std::vector<int>> spacingTbl;
    std::map<uint, std::pair<int, int>> excludedMap;
    rule->getTable(widthIdx, lengthIdx, spacingTbl, excludedMap);
    assert(excludedMap.size() <= 0);
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> widthPrlToSpacings;
    for (auto width : widthIdx) {
      for (auto prl : lengthIdx) {
        auto spacing = rule->getSpacing(width, prl);
        widthPrlToSpacings[std::make_pair((uint32_t) width, (uint32_t) prl)]
            = (uint32_t) spacing;
      }
    }
    for (auto [pair, spacing] : widthPrlToSpacings) {
      widths.insert(pair.first);
      prls.insert(pair.second);
    }
    subRule = RoutingSpacingTableRulePrl(std::move(widthPrlToSpacings));
  };

  ~RoutingSpacingTableRule(){};

  void widthAndPrlToSpacing(
      std::function<void(uint32_t width, uint32_t prl, uint32_t sapcing)>
          callback);

  uint64_t getSpacing(uint64_t width, uint64_t prl);

  uint64_t getMinSpacing();

  std::set<uint32_t> getWidths() const { return widths; }

  std::set<uint32_t> getPrls() const { return prls; }

  std::variant<std::monostate,
               RoutingSpacingTableRulePrl,
               RoutingSpacingTableRuleTwoWidth>
  getSubRule() const
  {
    return subRule;
  }

  friend std::ostream& operator<<(std::ostream& out,
                                  const RoutingSpacingTableRule& obj)
  {
    out << obj.layer->getName() << "\tSpacingTable";
    auto srValue = obj.subRule;
    std::visit(
        overloaded{
            [](std::monostate&) {},
            [&out](RoutingSpacingTableRulePrl& prl) { out << "\n\t" << prl; },
            [&out](RoutingSpacingTableRuleTwoWidth& width) {
              out << "\n\t" << width;
            },
        },
        srValue);
    return out;
  };
};

}  // namespace banyan