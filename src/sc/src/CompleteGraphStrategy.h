#pragma once

#include "ConstructStrategy.h"

namespace sc {
class CompleteGraphStrategy : public ConstructStrategy {
 public:
  void construct(Graph *const &graph) const override;
};
} // namespace sc

