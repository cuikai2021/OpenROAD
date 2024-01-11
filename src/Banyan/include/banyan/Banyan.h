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
  void clearWires(bool excludeClock, bool excludeSignal);
  void regression_RC(const char* spef,
                     const char* setRC_tcl,
                     const char* plot_file_prefix,
                     const char* corner_name);
  void savedb(std::string dbfile);
  void loaddb(std::string dbfile);
  // From ord::OpenRoad::Observer
  virtual void postReadLef(const char* filename,
                           odb::dbTech* tech,
                           odb::dbLib*) override
  {
    if (tech) {
      tech_lef_file_name_ = filename;
    } else {
      other_lef_file_names_.emplace_back(filename);
    }
  }

 protected:
  odb::dbDatabase* db_;
  stt::SteinerTreeBuilder* stt_builder_;
  utl::Logger* logger_;
  lrl::Laurel* laurel_;
  rsz::Resizer* resizer_;
  std::string tech_lef_file_name_;
  std::vector<std::string> other_lef_file_names_;
};  // class Banyan

}  // namespace banyan

#endif
