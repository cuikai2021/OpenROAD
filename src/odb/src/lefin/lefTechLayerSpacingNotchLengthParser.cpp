#include "boostParser.h"

#include "odb/db.h"
#include "lefLayerPropParser.h"
#include "odb/lefin.h"

using namespace odb;

namespace lefTechLayerSpacingNotchLength {

static void memberParser1(void (odb::dbTechLayerSpacingNotchLengthRule::*setter)(int),
                          odb::dbTechLayerSpacingNotchLengthRule* sc,
                          odb::lefinReader* l,
				          double value)
{
  (sc->*setter)(l->dbdist(value));
}

static void memberParser2(void (odb::dbTechLayerSpacingNotchLengthRule::*setter)(int, int),
                          odb::dbTechLayerSpacingNotchLengthRule* sc,
                          odb::lefinReader* l,
						  const boost::fusion::vector<double, double> params)
{
  (sc->*setter)(l->dbdist(at_c<0>(params)), l->dbdist(at_c<1>(params)));
}

#define SET1_(setter) \
 [boost::bind(&memberParser1, &TRule::setter, sc, l, _1)]
#define SET2_(setter) \
 [boost::bind(&memberParser2, &TRule::setter, sc, l, _1)]

template <typename Iterator>
bool parse(Iterator first, Iterator last,
		   odb::dbTechLayer* layer, odb::lefinReader* l)
{
  typedef odb::dbTechLayerSpacingNotchLengthRule TRule;
  TRule* sc = TRule::create(layer);
  typedef qi::rule<Iterator, space_type> Rule;

  Rule within = ((lit("WITHIN") >> double_ >> lit("SPANLENGTH") >> double_) SET2_(setWithin));
  Rule width = ((lit("WIDTH") >> double_) SET1_(setWidth));
  Rule concaveends = ((lit("CONCAVEENDS") >> double_)SET1_(setConcaveEnds));
  Rule notchwidth = ((lit("NOTCHWIDTH")  >> double_)SET1_(setNotchWidth));
  Rule others = within | width | concaveends | notchwidth;
  Rule exceptWithin = (lit("EXCEPTWITHIN")  >> double_ >> double_)SET2_(setExceptWithin);
  Rule spacingRule
   = (lit("SPACING")       >> (double_ SET1_(setMinSpace))
	 >> lit("NOTCHLENGTH") >> (double_ SET1_(setMinNotchLength))
	 >> -exceptWithin
	 >> -others
	 >> -lit(";"));
  bool valid
	  = qi::phrase_parse(first, last, spacingRule, space) && first == last;
  if (!valid)
	odb::dbTechLayerSpacingNotchLengthRule::destroy(sc);
  return valid;
}

} // namespace lefTechLayerSpacingNotchLength

namespace odb {

void lefTechLayerSpacingNotchLengthParser::parse(std::string s,
		                                         dbTechLayer* layer,
												 odb::lefinReader* l)
{
  std::vector<std::string> rules;
  boost::split(rules, s, boost::is_any_of(";"));
  for (auto rule : rules) {
    boost::algorithm::trim(rule);
    if (rule.empty())
	  continue;
    if (!lefTechLayerSpacingNotchLength::parse(rule.begin(), rule.end(), layer, l))
	  l->warning(
		255,
		"parse mismatch in layer propery LEF58_SPACING for layer {} :\"{}\"",
		layer->getName(),
		rule);
  }
}

} // namespace odb
