#include "banyan/MakeBanyan.h"

#include "ord/OpenRoad.hh"
#include "banyan/Banyan.h"

namespace ord {

banyan::Banyan* makeBanyan()
{
  return new banyan::Banyan();
}

void deleteBanyan(banyan::Banyan* router)
{
  delete router;
}

void initBanyan(OpenRoad* openroad)
{
  openroad->getBanyan()->init(
      openroad->tclInterp(), 
      openroad->getDb(), 
      openroad->getSteinerTreeBuilder(),
      openroad->getLogger(), 
      openroad->getLaurel(),
      openroad->getResizer());
  openroad->addObserver(openroad->getBanyan());
}

}  // namespace ord
