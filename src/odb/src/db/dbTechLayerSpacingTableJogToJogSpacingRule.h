#pragma once

#include <memory>
#include <vector>
#include "dbCore.h"
#include "dbId.h"
#include "dbTypes.h"
#include "odb.h"

namespace odb {

class _dbTechLayer;

class _dbTechLayerSpacingTableJogToJogSpacingRule : public _dbObject
{
public:
  uint jogtojog_spacing_;
  uint jog_width_;
  uint short_jog_spacing_;

  struct TableItem {
	  uint width_;
	  uint par_length_;
	  uint par_within_;
	  uint long_jog_spacing_;

	  uint short_jog_spacing_;
	  uint low_exclude_spacing_;
	  uint high_exclude_spacing_;
	  bool has_exceptwithin_ : 1;
	  bool has_shortjog_spacing_ : 1;
	  uint spare_bits : 32 - 2;
  };

  std::vector<std::shared_ptr<TableItem> > tbl_;
  // User Code Begin Fields
  dbId<_dbTechLayer> _layer;
  // User Code End Fields

  _dbTechLayerSpacingTableJogToJogSpacingRule(_dbDatabase*,
    const _dbTechLayerSpacingTableJogToJogSpacingRule& r);
  _dbTechLayerSpacingTableJogToJogSpacingRule(_dbDatabase*);
  ~_dbTechLayerSpacingTableJogToJogSpacingRule();
  bool operator==(const _dbTechLayerSpacingTableJogToJogSpacingRule& rhs) const;
  bool operator!=(const _dbTechLayerSpacingTableJogToJogSpacingRule& rhs) const
  {
    return !operator==(rhs);
  }
  bool operator<(const _dbTechLayerSpacingTableJogToJogSpacingRule& rhs) const;
  void differences(dbDiff& diff,
				 const char* field,
				 const _dbTechLayerSpacingTableJogToJogSpacingRule& rhs) const;
  void out(dbDiff& diff, char side, const char* field) const;
};

dbIStream& operator>>(dbIStream& stream, _dbTechLayerSpacingTableJogToJogSpacingRule& obj);
dbOStream& operator<<(dbOStream& stream,
                      const _dbTechLayerSpacingTableJogToJogSpacingRule& obj);

} // namespace odb
