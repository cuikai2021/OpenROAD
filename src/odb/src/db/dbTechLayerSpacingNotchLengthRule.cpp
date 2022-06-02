#include "dbTechLayerSpacingNotchLengthRule.h"

#include "db.h"
#include "dbDatabase.h"
#include "dbDiff.hpp"
#include "dbTable.h"
#include "dbTable.hpp"

// User Code Begin Includes
#include "dbTech.h"
#include "dbTechLayer.h"
// User Code End Includes
namespace odb {

template class dbTable<_dbTechLayerSpacingNotchLengthRule>;

bool _dbTechLayerSpacingNotchLengthRule::operator==(
	 const _dbTechLayerSpacingNotchLengthRule& rhs) const
{
  if (*(uint*)&flags_ != *(uint*)&rhs.flags_) return false;
  return true;
}

bool _dbTechLayerSpacingNotchLengthRule::operator<(
	 const _dbTechLayerSpacingNotchLengthRule& rhs) const
{
  return true;
}

void _dbTechLayerSpacingNotchLengthRule::differences(
    dbDiff& diff,
    const char* field,
    const _dbTechLayerSpacingNotchLengthRule& rhs) const
{
  DIFF_BEGIN

  DIFF_FIELD(flags_.has_except_within_);
  DIFF_FIELD(flags_.has_within_);
  DIFF_FIELD(flags_.has_width_);
  DIFF_FIELD(flags_.has_concaveends_);
  DIFF_FIELD(flags_.has_notchwidth_);

  DIFF_FIELD(min_space_);
  DIFF_FIELD(min_notch_length_);
  DIFF_FIELD(low_exclude_space_);
  DIFF_FIELD(high_exclude_space_);
  DIFF_FIELD(value1_);
  DIFF_FIELD(value2_);

  DIFF_END
}

void _dbTechLayerSpacingNotchLengthRule::out(dbDiff& diff,
	                                         char side,
                                             const char* field) const
{
  DIFF_OUT_BEGIN

  DIFF_OUT_FIELD(flags_.has_except_within_);
  DIFF_OUT_FIELD(flags_.has_within_);
  DIFF_OUT_FIELD(flags_.has_width_);
  DIFF_OUT_FIELD(flags_.has_concaveends_);
  DIFF_OUT_FIELD(flags_.has_notchwidth_);

  DIFF_OUT_FIELD(min_space_);
  DIFF_OUT_FIELD(min_notch_length_);
  DIFF_OUT_FIELD(low_exclude_space_);
  DIFF_OUT_FIELD(high_exclude_space_);
  DIFF_OUT_FIELD(value1_);
  DIFF_OUT_FIELD(value2_);

  DIFF_END
}

_dbTechLayerSpacingNotchLengthRule::_dbTechLayerSpacingNotchLengthRule(_dbDatabase* db)
{
  (*(uint32_t*)&flags_) = 0;
  min_space_ = 0;
  min_notch_length_ = 0;
  low_exclude_space_ = 0;
  high_exclude_space_ = 0;
  value1_ = 0;
  value2_ = 0;
}

_dbTechLayerSpacingNotchLengthRule::_dbTechLayerSpacingNotchLengthRule(_dbDatabase* db,
  const _dbTechLayerSpacingNotchLengthRule& r)
{
  (*(uint32_t*)&flags_) = (*(uint32_t*)&r.flags_);
  min_space_            = r.min_space_;
  min_notch_length_     = r.min_notch_length_;
  low_exclude_space_    = r.low_exclude_space_;
  high_exclude_space_   = r.high_exclude_space_;
  value1_               = r.value1_;
  value2_               = r.value2_;
}

dbIStream& operator>>(dbIStream& stream, _dbTechLayerSpacingNotchLengthRule& obj)
{
  stream >> (*(uint32_t*)&obj.flags_);
  stream >> obj.min_space_;
  stream >> obj.min_notch_length_;
  stream >> obj.low_exclude_space_;
  stream >> obj.high_exclude_space_;
  stream >> obj.value1_;
  stream >> obj.value2_;
  return stream;
}

dbOStream& operator<<(dbOStream& stream, const _dbTechLayerSpacingNotchLengthRule& obj)
{
  stream << (*(uint32_t*)&obj.flags_);
  stream << obj.min_space_;
  stream << obj.min_notch_length_;
  stream << obj.low_exclude_space_;
  stream << obj.high_exclude_space_;
  stream << obj.value1_;
  stream << obj.value2_;
  return stream;
}

_dbTechLayerSpacingNotchLengthRule::~_dbTechLayerSpacingNotchLengthRule() {}

#define internal() \
  (*(_dbTechLayerSpacingNotchLengthRule*)this)

void dbTechLayerSpacingNotchLengthRule::setMinSpace(int min_space)
{
  internal().min_space_ = min_space;
}

uint dbTechLayerSpacingNotchLengthRule::getMinSpace() const
{
  return internal().min_space_;
}

bool dbTechLayerSpacingNotchLengthRule::hasNotchWidth() const
{
  return internal().flags_.has_notchwidth_;
}

void dbTechLayerSpacingNotchLengthRule::setNotchWidth(int notch_width)
{
  auto thiz = internal();
  thiz.flags_.has_notchwidth_ = true;
  thiz.value1_ = notch_width;
}

uint dbTechLayerSpacingNotchLengthRule::getNotchWidth() const
{
  assert(internal().flags_.has_notchwidth_);
  return internal().value1_;
}

void dbTechLayerSpacingNotchLengthRule::setMinNotchLength(int min_notch_length)
{
  internal().min_notch_length_ = min_notch_length;
}

uint dbTechLayerSpacingNotchLengthRule::getMinNotchLength() const
{
  return internal().min_notch_length_;
}

void dbTechLayerSpacingNotchLengthRule::setExceptWithin(int low_exclude_space, int high_exclude_space)
{
  auto thiz = internal();
  thiz.flags_.has_except_within_ = true;
  thiz.high_exclude_space_ = high_exclude_space;
  thiz.low_exclude_space_  = low_exclude_space;
}

uint dbTechLayerSpacingNotchLengthRule::getExceptWithinHighExcludeSpace() const
{
  return internal().high_exclude_space_;
}

uint dbTechLayerSpacingNotchLengthRule::getExceptWithinLowExcludeSpace() const
{
  return internal().low_exclude_space_;
}

bool dbTechLayerSpacingNotchLengthRule::hasExceptWithin() const
{
  return internal().flags_.has_except_within_;
}

void dbTechLayerSpacingNotchLengthRule::setWithin(int within, int sideof_notch_span_length)
{
  auto thiz = internal();
  thiz.flags_.has_within_ = true;
  thiz.value2_ = within;
  thiz.value1_ = sideof_notch_span_length;
}

bool dbTechLayerSpacingNotchLengthRule::hasWithin() const
{
  return internal().flags_.has_within_;
}

uint dbTechLayerSpacingNotchLengthRule::getWithinDistance() const
{
  assert(internal().flags_.has_within_);
  return internal().value2_;
}

uint dbTechLayerSpacingNotchLengthRule::getWithinSideofNotchSpanLength() const
{
  assert(internal().flags_.has_within_);
  return internal().value1_;
}

void dbTechLayerSpacingNotchLengthRule::setWidth(int sideof_notch_width)
{
  auto thiz = internal();
  thiz.flags_.has_width_ = true;
  thiz.value1_ = sideof_notch_width;
}

bool dbTechLayerSpacingNotchLengthRule::hasWidth() const
{
  return internal().flags_.has_width_;
}

uint dbTechLayerSpacingNotchLengthRule::getWidth() const
{
  assert(hasWidth());
  return internal().value1_;
}

void dbTechLayerSpacingNotchLengthRule::setConcaveEnds(int sideof_notch_width)
{
  auto thiz = internal();
  thiz.flags_.has_concaveends_ = true;
  thiz.value1_ = sideof_notch_width;
}

bool dbTechLayerSpacingNotchLengthRule::hasConcaveEnds() const
{
  return internal().flags_.has_concaveends_;
}

uint dbTechLayerSpacingNotchLengthRule::getConcaveEnds() const
{
  assert(hasConcaveEnds());
  return internal().value1_;
}

#undef internal

// User Code Begin dbTechLayerSpacingEolRulePublicMethods
dbTechLayerSpacingNotchLengthRule* dbTechLayerSpacingNotchLengthRule::create(
    dbTechLayer* _layer)
{
  _dbTechLayer* layer = (_dbTechLayer*) _layer;
  _dbTechLayerSpacingNotchLengthRule* newrule = layer->spacing_notch_length_rules_tbl_->create();
  newrule->_layer = _layer->getImpl()->getOID();

  return ((dbTechLayerSpacingNotchLengthRule*) newrule);
}

dbTechLayerSpacingNotchLengthRule*
dbTechLayerSpacingNotchLengthRule::getTechLayerSpacingNotchLengthRule(dbTechLayer* inly,
                                                      uint dbid)
{
  _dbTechLayer* layer = (_dbTechLayer*) inly;
  return (dbTechLayerSpacingNotchLengthRule*) layer->spacing_notch_length_rules_tbl_->getPtr(
      dbid);
}

void dbTechLayerSpacingNotchLengthRule::destroy(dbTechLayerSpacingNotchLengthRule* rule)
{
  _dbTechLayer* layer = (_dbTechLayer*) rule->getImpl()->getOwner();
  dbProperty::destroyProperties(rule);
  layer->spacing_notch_length_rules_tbl_->destroy((_dbTechLayerSpacingNotchLengthRule*) rule);
}

} // namespace odb
