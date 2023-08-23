#pragma once

#include "gpl/initialPlaceBicgstab.h"

class Matrix;

namespace utl {
class Logger;
}

namespace gpl {

class PlacerBase;
class Graphics;

class InitialPlaceDirichlet : public InitialPlaceBicgstab
{
 public:
  InitialPlaceDirichlet() {}
  InitialPlaceDirichlet(InitialPlaceVars ipVars,
                        std::shared_ptr<PlacerBase> pb,
                        utl::Logger* logger)
      : InitialPlaceBicgstab(ipVars, pb, logger)
  {
  }
  ~InitialPlaceDirichlet() {}

  void run();

 protected:
  void updateInstLoc(const Matrix& eig_vecs, DIMENSION dim);
};

}  // namespace gpl