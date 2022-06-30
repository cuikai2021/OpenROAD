#pragma once

namespace gpl {
class LindenGlobalPlace;
}

namespace ord {

class OpenRoad;

gpl::LindenGlobalPlace*
makeLindenGlobalPlace();

void
initLindenGlobalPlace(OpenRoad* openroad);

void
deleteLindenGlobalPlace(gpl::LindenGlobalPlace *LindenGlobalPlace);

}
