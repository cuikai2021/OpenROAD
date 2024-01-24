#include "dbTechLayerSpacingTableJogToJogSpacingRule.h"

#include "db.h"
#include "dbDatabase.h"
#include "dbDiff.hpp"
#include "dbTable.h"
#include "dbTable.hpp"
#include "dbTechLayer.h"

namespace odb {

_dbTechLayerSpacingTableJogToJogSpacingRule::_dbTechLayerSpacingTableJogToJogSpacingRule(_dbDatabase*,
  const _dbTechLayerSpacingTableJogToJogSpacingRule& r)
{
  jogtojog_spacing_  = r.jogtojog_spacing_;
  jog_width_         = r.jog_width_;
  short_jog_spacing_ = r.short_jog_spacing_;
  tbl_               = r.tbl_;
}

_dbTechLayerSpacingTableJogToJogSpacingRule::_dbTechLayerSpacingTableJogToJogSpacingRule(_dbDatabase*)
{
  jogtojog_spacing_  = 0;
  jog_width_         = 0;
  short_jog_spacing_ = 0;
}

_dbTechLayerSpacingTableJogToJogSpacingRule::~_dbTechLayerSpacingTableJogToJogSpacingRule()
{
}

bool _dbTechLayerSpacingTableJogToJogSpacingRule::operator==(const _dbTechLayerSpacingTableJogToJogSpacingRule& r) const
{
  if (jogtojog_spacing_  != r.jogtojog_spacing_) return false;
  if (jog_width_         != r.jog_width_)          return false;
  if (short_jog_spacing_ != r.short_jog_spacing_)  return false;
  if (tbl_.size()        != r.tbl_.size())         return false;
  for (size_t i = 0; i < tbl_.size(); ++i) {
	if (tbl_[i]->width_                != r.tbl_[i]->width_) return false;
	if (tbl_[i]->par_length_           != r.tbl_[i]->par_length_) return false;
	if (tbl_[i]->par_within_           != r.tbl_[i]->par_within_) return false;
	if (tbl_[i]->long_jog_spacing_     != r.tbl_[i]->long_jog_spacing_) return false;

	if (tbl_[i]->has_exceptwithin_     != r.tbl_[i]->has_exceptwithin_) return false;
	if (tbl_[i]->has_exceptwithin_) {
	  if (tbl_[i]->low_exclude_spacing_!= r.tbl_[i]->low_exclude_spacing_) return false;
	  if (tbl_[i]->high_exclude_spacing_!= r.tbl_[i]->high_exclude_spacing_) return false;
	}
	if (tbl_[i]->has_shortjog_spacing_ != r.tbl_[i]->has_shortjog_spacing_) return false;
	if (tbl_[i]->has_shortjog_spacing_)
	  if (tbl_[i]->short_jog_spacing_  != r.tbl_[i]->short_jog_spacing_) return false;
  }
  return true;
}

bool _dbTechLayerSpacingTableJogToJogSpacingRule::operator<(const _dbTechLayerSpacingTableJogToJogSpacingRule& rhs) const
{
  return true;
}

void _dbTechLayerSpacingTableJogToJogSpacingRule::differences(dbDiff& diff,
  const char* field, const _dbTechLayerSpacingTableJogToJogSpacingRule& rhs) const
{
  DIFF_BEGIN
  DIFF_FIELD(jogtojog_spacing_);
  DIFF_FIELD(jog_width_);
  DIFF_FIELD(short_jog_spacing_);
  DIFF_END
}

void _dbTechLayerSpacingTableJogToJogSpacingRule::out(dbDiff& diff, char side, const char* field) const
{
  DIFF_OUT_BEGIN
  DIFF_OUT_FIELD(jogtojog_spacing_);
  DIFF_OUT_FIELD(jog_width_);
  DIFF_OUT_FIELD(short_jog_spacing_);
  DIFF_END
}

#define internal() \
  (*(_dbTechLayerSpacingTableJogToJogSpacingRule*)this)

void dbTechLayerSpacingTableJogToJogSpacingRule::setJogToJogSpacing(int jogtojog_spacing)
{
  internal().jogtojog_spacing_ = jogtojog_spacing;
}

uint dbTechLayerSpacingTableJogToJogSpacingRule::getJogToJogSpacing() const
{
  return internal().jogtojog_spacing_;
}

void dbTechLayerSpacingTableJogToJogSpacingRule::setJogWidth(int jog_width)
{
  internal().jog_width_ = jog_width;
}

uint dbTechLayerSpacingTableJogToJogSpacingRule::getJogWidth() const
{
  return internal().jog_width_;
}

void dbTechLayerSpacingTableJogToJogSpacingRule::setShortJogSpacing(int shortjog_spacing)
{
  internal().short_jog_spacing_ = shortjog_spacing;
}

uint dbTechLayerSpacingTableJogToJogSpacingRule::getShortJogSpacing() const
{
  return internal().short_jog_spacing_;
}

void dbTechLayerSpacingTableJogToJogSpacingRule::appendWidth(
	int width, int parLength, int parWithin, int longjogspacing,
	bool has_exceptwithin, int low, int high,
	bool has_shortjogspacing, int shortjogspacing)
{
  typedef _dbTechLayerSpacingTableJogToJogSpacingRule::TableItem Item;
  Item* i = new Item {(uint)width, (uint)parLength, (uint)parWithin,
    (uint)longjogspacing, (uint)shortjogspacing, (uint)low, (uint)high,
	has_exceptwithin, has_shortjogspacing, 0};
  internal().tbl_.push_back(std::shared_ptr<Item>(i));
}

uint dbTechLayerSpacingTableJogToJogSpacingRule::tableSize() const
{
  return internal().tbl_.size();
}

#define item(index) \
  (*(internal().tbl_[index]))

uint dbTechLayerSpacingTableJogToJogSpacingRule::getWidth(uint index) const
{
  return item(index).width_;
}

uint dbTechLayerSpacingTableJogToJogSpacingRule::getParLength(uint index) const
{
  return item(index).par_length_;
}

uint dbTechLayerSpacingTableJogToJogSpacingRule::getParWithin(uint index) const
{
  return item(index).par_within_;
}

uint dbTechLayerSpacingTableJogToJogSpacingRule::getLongJogSpacing(uint index) const
{
  return item(index).long_jog_spacing_;
}

bool dbTechLayerSpacingTableJogToJogSpacingRule::hasExceptWithin(uint index) const
{
  return item(index).has_exceptwithin_;
}

uint dbTechLayerSpacingTableJogToJogSpacingRule::getExceptWithinLowExcludeSpace(uint index) const
{
  return item(index).low_exclude_spacing_;
}

uint dbTechLayerSpacingTableJogToJogSpacingRule::getExceptWithinHighExcludeSpace(uint index) const
{
  return item(index).high_exclude_spacing_;
}

bool dbTechLayerSpacingTableJogToJogSpacingRule::hasShortJogSpacing(uint index) const
{
  return item(index).has_shortjog_spacing_;
}

#undef internal

dbTechLayerSpacingTableJogToJogSpacingRule* dbTechLayerSpacingTableJogToJogSpacingRule::create(dbTechLayer* _layer)
{
  _dbTechLayer* layer = (_dbTechLayer*) _layer;
  _dbTechLayerSpacingTableJogToJogSpacingRule* newrule
	  = layer->spacing_table_jogtojog_spacing_rules_tbl_->create();
  return ((dbTechLayerSpacingTableJogToJogSpacingRule*) newrule);
}

dbTechLayerSpacingTableJogToJogSpacingRule* dbTechLayerSpacingTableJogToJogSpacingRule::getTechLayerSpacingTableJogToJogSpacingRule(
        dbTechLayer* inly,
        uint dbid)
{
  _dbTechLayer* layer = (_dbTechLayer*) inly;
  return (dbTechLayerSpacingTableJogToJogSpacingRule*)
    layer->spacing_table_jogtojog_spacing_rules_tbl_->getPtr(dbid);
}

void dbTechLayerSpacingTableJogToJogSpacingRule::destroy(dbTechLayerSpacingTableJogToJogSpacingRule* rule)
{
  _dbTechLayer* layer = (_dbTechLayer*) rule->getImpl()->getOwner();
  dbProperty::destroyProperties(rule);
  layer->spacing_table_jogtojog_spacing_rules_tbl_->destroy(
    (_dbTechLayerSpacingTableJogToJogSpacingRule*) rule);
}


} // namespace odb
