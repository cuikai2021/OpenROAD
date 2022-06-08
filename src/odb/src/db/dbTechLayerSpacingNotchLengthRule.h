#pragma once

#include "dbCore.h"
#include "dbId.h"
#include "dbTypes.h"
#include "odb.h"

namespace odb {

class _dbTechLayer;

struct dbTechLayerSpacingNotchLengthRuleFlags
{
  bool has_except_within_ : 1;
  bool has_within_ : 1;
  bool has_width_ : 1;
  bool has_concaveends_ : 1;
  bool has_notchwidth_ : 1;
  uint spare_bits_ : 32 - 5;
};

class _dbTechLayerSpacingNotchLengthRule : public _dbObject
{
public:
  dbTechLayerSpacingNotchLengthRuleFlags flags_;
  uint min_space_;
  uint min_notch_length_;
  // exceptwithin related
  uint low_exclude_space_;
  uint high_exclude_space_;

  uint value1_; // sideof notch span length, sideof notch width, notch width
  uint value2_; // within

  // User Code Begin Fields
  dbId<_dbTechLayer> _layer;
  // User Code End Fields
  _dbTechLayerSpacingNotchLengthRule(_dbDatabase*, const _dbTechLayerSpacingNotchLengthRule& );
  _dbTechLayerSpacingNotchLengthRule(_dbDatabase*);
  ~_dbTechLayerSpacingNotchLengthRule();
  bool operator==(const _dbTechLayerSpacingNotchLengthRule& rhs) const;
  bool operator!=(const _dbTechLayerSpacingNotchLengthRule& rhs) const { return !operator==(rhs); }
  bool operator<(const _dbTechLayerSpacingNotchLengthRule& rhs) const;
  void differences(dbDiff& diff,
		           const char* field,
				   const _dbTechLayerSpacingNotchLengthRule& rhs) const;
  void out(dbDiff& diff, char side, const char* field) const;
};

dbIStream& operator>>(dbIStream& stream, _dbTechLayerSpacingNotchLengthRule& obj);
dbOStream& operator<<(dbOStream& stream, const _dbTechLayerSpacingNotchLengthRule& obj);

} // namespace odb
