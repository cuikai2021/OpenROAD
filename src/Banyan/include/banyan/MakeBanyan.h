#pragma once

namespace banyan {
class Banyan;
}

namespace ord {

class OpenRoad;

banyan::Banyan* makeBanyan();

void deleteBanyan(banyan::Banyan* router);

void initBanyan(OpenRoad* openroad);

} // namespace ord
