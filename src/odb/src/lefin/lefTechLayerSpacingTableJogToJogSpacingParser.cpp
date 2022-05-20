#include "boostParser.h"
#include <iostream>
#include <string>

#include "db.h"
#include "lefLayerPropParser.h"
#include "lefin.h"

namespace lefTechLayerSpacingTableJogToJogSpacing {

static void memberParser1(void (odb::dbTechLayerSpacingTableJogToJogSpacingRule::*setter)(int),
                          odb::dbTechLayerSpacingTableJogToJogSpacingRule* sc,
                          odb::lefin* l,
				          double value)
{
  (sc->*setter)(l->dbdist(value));
}

#define SET1_(setter) \
 [boost::bind(&memberParser1, &TRule::setter, sc, l, _1)]

#if 0
struct Stack {
  bool   has_exceptwithin;
  double exceptwithin_low;
  double exceptwithin_high;
  bool   has_shortjogspacing;
  double shortjogspacing;
};
#endif

#if 1
static void appendWidth(odb::dbTechLayerSpacingTableJogToJogSpacingRule* sc,
                        odb::lefin* l,
						boost::fusion::vector< double, double, double,
						  boost::optional< boost::fusion::vector<double, double> >,
						  double, boost::optional< double > >& params)
{
  auto except = at_c<3>(params);
  bool has_except = false;
  uint except_low  = 0;
  uint except_high = 0;
  if (except.is_initialized()) {
	has_except = true;
	auto val = except.value();
	except_low  = l->dbdist(at_c<0>(val));
	except_high = l->dbdist(at_c<1>(val));
  }
  auto sjs = at_c<5>(params);
  bool has_shortjogspacing = false;
  uint shortjogspacing = 0;
  if (sjs.is_initialized()) {
	has_shortjogspacing = true;
	shortjogspacing = l->dbdist(sjs.value());
  }

  sc->appendWidth(l->dbdist(at_c<0>(params)),
		          l->dbdist(at_c<1>(params)),
				  l->dbdist(at_c<2>(params)),
				  l->dbdist(at_c<4>(params)),
				  has_except, except_low, except_high,
				  has_shortjogspacing, shortjogspacing);
}
#endif

template <typename Iterator>
bool parse(Iterator first, Iterator last,
		   odb::dbTechLayer* layer, odb::lefin* l)
{
  typedef odb::dbTechLayerSpacingTableJogToJogSpacingRule TRule;
  TRule* sc = TRule::create(layer);
  typedef qi::rule<Iterator, space_type> Rule;
  Rule spacingtable
   = +((lit("WIDTH") >> double_
	  >> lit("PARALLEL") >> double_
	  >> lit("WITHIN") >> double_
	  >> -(lit("EXCEPTWITHIN")  >> double_ >> double_)
	  >> lit("LONGJOGSPACING") >> double_
	  >> -(lit("SHORTJOGSPACING") >> double_)
	 )[boost::bind(&appendWidth, sc, l, _1)]);
  Rule spacingtableRule
   = (lit("SPACINGTABLE")
    >> lit("JOGTOJOGSPACING") >> (double_ SET1_(setJogToJogSpacing))
    >> lit("JOGWIDTH") >> (double_ SET1_(setJogWidth))
	>> lit("SHORTJOGSPACING") >> (double_ SET1_(setShortJogSpacing))
	>> -spacingtable
    >> -lit(";"));
  bool valid
	  = qi::phrase_parse(first, last, spacingtableRule, space) && first == last;
  if (!valid)
	odb::dbTechLayerSpacingTableJogToJogSpacingRule::destroy(sc);
  return valid;
}

} // namespace lefTechLayerSpacingTableJogToJogSpacing

namespace odb {

bool lefTechLayerSpacingTableJogToJogSpacingParser::parse(std::string s,
														  dbTechLayer* layer,
		                                                  odb::lefin* l)
{
  return lefTechLayerSpacingTableJogToJogSpacing::parse(s.begin(), s.end(), layer, l);
}

}
