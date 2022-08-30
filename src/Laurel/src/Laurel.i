%{

#include "lrl/Laurel.hh"
#include "lrl/Options.hh"
#include "sta/Sta.hh"
#include "sta/MinMax.hh"

namespace ord {
// Defined in OpenRoad.i
lrl::Laurel *
getLaurel();

void
ensureLinked();
}

using sta::Sta;
using sta::MinMax;

using lrl::Laurel;
using lrl::Options;

using ord::getLaurel;
using ord::ensureLinked;

%}

%typemap(in) MinMax* {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  MinMax *min_max = MinMax::find(arg);
  if (min_max)
    $1 = min_max;
  else
    return TCL_ERROR;
}

%include "../../Exception.i"

%inline %{

// ICBench methods

namespace lrl {

int
fix_hold_cmd(bool multi_mode)
{
  ensureLinked();
  Laurel *laurel = getLaurel();
  return laurel->fixHold(multi_mode);
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

int
fix_setup_cmd(int iter)
{
  ensureLinked();
  Laurel *laurel = getLaurel();
  return laurel->fixSetup(iter);
}

int
ccopt_cmd()
{
  ensureLinked();
  Laurel *laurel = getLaurel();
  return laurel->ccopt();
}

void
wire_budgeting_cmd()
{
  ensureLinked();
  Laurel *laurel = getLaurel();
  laurel->wireBudgeting();
}

int
fast_opt_cmd(int iter)
{
  ensureLinked();
  Laurel *laurel = getLaurel();
  return laurel->fastOpt(iter);
}

void
report_violations_cmd(MinMax *min_max,
                      bool ignore_top_ports)
{
  ensureLinked();
  Laurel *laurel = getLaurel();
  return laurel->reportViolations(min_max, ignore_top_ports);
}

} // namespace

%} // inline
