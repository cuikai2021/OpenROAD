#include "gpl/initialPlaceDef.h"

#include "gpl/placerBase.h"
#include "utl/Logger.h"

namespace gpl {
using namespace std;

// initial place with positions from the DEF file
void InitialPlaceDef::run()
{
  // do nothing, as all initial positions are given through the def file.
  //
  // NOTE:
  //    other initial placer starts from placeInstsCenter() which reset all
  //    cells to die's center position.
  log_->report("Initial place with def file, start HPWL={}", pb_->hpwl());
  return;
}

}  // namespace gpl
