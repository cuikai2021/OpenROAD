#ifndef _BANYAN_H_
#define _BANYAN_H_

#include <dlfcn.h>
#include <tcl.h>

#include <boost/algorithm/string.hpp>
#include <boost/process.hpp>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "ByDb.hpp"
#include "ord/OpenRoad.hh"
#include "odb/db.h"

namespace odb {
class dbDatabase;
}

namespace utl {
class Logger;
}

namespace lrl {
class Laurel;
}

namespace stt {
class SteinerTreeBuilder;
}

namespace banyan {
using byvia = std::tuple<int, int, std::string>;

constexpr const std::string_view BANYAN_TMP_FOLDER = "/tmp/banyan/";

class Banyan : public ord::OpenRoad::Observer
{
 public:
  Banyan();
  ~Banyan();
  void init(Tcl_Interp* tcl_interp,
            odb::dbDatabase* db,
            stt::SteinerTreeBuilder* stt_builder,
            utl::Logger* logger,
            lrl::Laurel* laurel,
            rsz::Resizer* resizer);

  int main(std::vector<std::string> args, bool updateDB);
  int cmap(const std::string& cmapPb, bool cgr_mode);
  void mergeWireSegment();
  int m1GridlessMaze();
  void runGui();
  void drcPatch();
  void clearWires(bool excludeClock, bool excludeSignal);
  void regression_RC(const char *spef, const char *setRC_tcl);
  // From ord::OpenRoad::Observer
  virtual void postReadLef(const char* filename,
                           odb::dbTech* tech,
                           odb::dbLib* library) override
  {
    if (tech) {
      techfile_name_ = filename;
    }
  }

 protected:
  odb::dbDatabase* db_;
  stt::SteinerTreeBuilder* stt_builder_;
  utl::Logger* logger_;
  lrl::Laurel* laurel_;
  rsz::Resizer* resizer_;
  std::string techfile_name_;
};  // class Banyan

}  // namespace banyan

#endif
