#pragma once

namespace cgr {
class CongestionRepair;
}

namespace ord {

class OpenRoad;

cgr::CongestionRepair*
makeCongestionRepair();

void
initCongestionRepair(OpenRoad* openroad);

void
deleteCongestionRepair(cgr::CongestionRepair *CongestionRepair);

}
