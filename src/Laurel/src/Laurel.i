%{

#include "lrl/Laurel.hh"
#include "lrl/Options.hh"

namespace ord {
// Defined in OpenRoad.i
lrl::Laurel *
getLaurel();

void
ensureLinked();
}

using lrl::Laurel;
using lrl::Options;

using ord::getLaurel;
using ord::ensureLinked;

%}

%include "../../Exception.i"

%inline %{

// ICBench methods

namespace lrl {

int
fix_hold_cmd()
{
  ensureLinked();
  Laurel *laurel = getLaurel();
  return laurel->fixHold();
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

int
fix_setup_cmd()
{
  ensureLinked();
  Laurel *laurel = getLaurel();
  return laurel->fixSetup();
}

} // namespace

%} // inline
