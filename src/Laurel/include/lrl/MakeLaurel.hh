#pragma once

namespace sta {
class dbSta;
class Laurel;
}  // namespace sta

namespace lrl {
class Laurel;
}  // namespace lrl

namespace ord {

class OpenRoad;

lrl::Laurel* makeLaurel();

void deleteLaurel(lrl::Laurel* laurel);

void initLaurel(OpenRoad* openroad);

}  // namespace ord
