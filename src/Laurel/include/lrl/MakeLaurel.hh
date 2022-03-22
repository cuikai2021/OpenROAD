#pragma once

namespace lrl {
class Laurel;
}  // namespace lrl

namespace ord {

class OpenRoad;

lrl::Laurel* makeLaurel();

void deleteLaurel(lrl::Laurel* laurel);

void initLaurel(OpenRoad* openroad);

}  // namespace ord
