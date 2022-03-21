%include "../../Exception.i"
%include "../../rsz/src/Resizer.i"

%{

#include "lrl/Laurel.hh"
#include "lrl/Options.hh"

namespace ord {
// Defined in OpenRoad.i
lrl::Laurel *
getLaurel();
}  // namespace

using ord::getLaurel;

using lrl::Laurel;
using lrl::Options;

%}

%inline %{

namespace lrl {

void
fix_hold_cmd()
{
  ensureLinked();
  Laurel *laurel = getLaurel();
  laurel->fixHold();
}

void
set_uncertainty(float uncertainty)
{
  Laurel *laurel = getLaurel();
  Options *options = laurel->getOptions();
  options->setUncertainty(uncertainty);
}

void
set_slack_margin(float slack_margin)
{
  Laurel *laurel = getLaurel();
  Options *options = laurel->getOptions();
  options->setSlackMargin(slack_margin);
}

void
analyze_clk()
{
  Laurel *laurel = getLaurel();
  Options *options = laurel->getOptions();
  options->analyzeClk();
}

void
fix_setup_cmd()
{
  ensureLinked();
  Laurel *laurel = getLaurel();
  laurel->fixSetup();
}

} // namespace

%} // inline
