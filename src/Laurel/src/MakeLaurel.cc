#include "lrl/MakeLaurel.hh"

#include "gui/gui.h"
#include "lrl/Laurel.hh"
#include "ord/OpenRoad.hh"

namespace ord {

lrl::Laurel* makeLaurel()
{
  return new lrl::Laurel;
}

void deleteLaurel(lrl::Laurel* laurel)
{
  delete laurel;
}

void initLaurel(OpenRoad* openroad)
{
  openroad->getLaurel()->init(openroad,
                              openroad->tclInterp(),
                              openroad->getLogger(),
                              // Broken gui api missing openroad accessor.
                              gui::Gui::get(),
                              openroad->getDb(),
                              openroad->getSta(),
                              openroad->getSteinerTreeBuilder(),
                              openroad->getGlobalRouter());
}

}  // namespace ord
