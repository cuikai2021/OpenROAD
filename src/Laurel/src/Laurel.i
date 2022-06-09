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
debug_report()
{
  Laurel *laurel = getLaurel();
  Options *options = laurel->getOptions();
  options->debugReport();
}

void
set_wire_bud_mode_rc()
{
  Laurel *laurel = getLaurel();
  Options *options = laurel->getOptions();
  options->setWireBudModeRC();
}

void
reset_wire_bud_mode()
{
  Laurel *laurel = getLaurel();
  Options *options = laurel->getOptions();
  options->resetWireBudMode();
}

void
set_spef_file(const char* filename)
{
  Laurel *laurel = getLaurel();
  Options *options = laurel->getOptions();
  options->setSpefFile(filename);
}

void
fix_setup_cmd()
{
  ensureLinked();
  Laurel *laurel = getLaurel();
  laurel->fixSetup();
}

void
wire_budgeting_cmd()
{
  ensureLinked();
  Laurel *laurel = getLaurel();
  laurel->wireBudgeting();
}

} // namespace

%} // inline
