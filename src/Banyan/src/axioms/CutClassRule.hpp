#pragma once

#include <assert.h>
#include <inttypes.h>

#include "axioms/Geometry.hpp"
#include "axioms/Via.hpp"
#include "odb/db.h"

namespace banyan {

template <typename T>
using CutClassMap = std::map<std::string, std::shared_ptr<T>>;
template <typename T>
using LayerCutClassRules = std::map<odb::dbTechLayer*, CutClassMap<T>>;

class CutClassRule
{
 private:
  odb::dbTechLayer* layer;
  std::string className;
  uint64_t width;
  std::optional<uint64_t> length;
  std::optional<uint64_t> cutNums;

 public:
  static LayerCutClassRules<CutClassRule> laToCutClassRules;

  CutClassRule(odb::dbTechLayer* layer,
               odb::dbTechLayerCutClassRule* odbCutClass)
      : layer(layer),width(odbCutClass->getWidth()),className(odbCutClass->getName()){
    if (odbCutClass->isLengthValid()) {
      length = std::make_optional<uint64_t>(odbCutClass->getLength());
    }
    if (odbCutClass->isCutsValid()) {
      cutNums = std::make_optional<uint64_t>(odbCutClass->getNumCuts());
    }
  };

  ~CutClassRule(){};

  std::string getClassName() const { return className; }

  bool contain(std::shared_ptr<Via> via);

  friend std::ostream& operator<<(std::ostream& out, const CutClassRule& obj)
  {
    out << obj.layer->getName() << "\tCUTCLASS " << obj.className << " WIDTH "
        << obj.width;
    if (obj.length.has_value()) {
      out << " LENGTH " << obj.length.value();
    }
    if (obj.cutNums.has_value()) {
      out << " CUTS " << obj.cutNums.value();
    }
    return out;
  }

  static std::optional<std::shared_ptr<CutClassRule>> findCutClass(
      odb::dbTechLayer* layer,
      std::string className);
};

}  // namespace banyan