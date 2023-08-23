#pragma once

#include "gpl/initialPlace.h"

namespace gpl {

class InitialPlaceDef : public InitialPlace {
  public:
    InitialPlaceDef(InitialPlaceVars ipVars, 
        std::shared_ptr<PlacerBase> pb,
        utl::Logger* logger) : InitialPlace(ipVars, pb, logger) {}
    ~InitialPlaceDef() {}

    virtual void run();
};

}