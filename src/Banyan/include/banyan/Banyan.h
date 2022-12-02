#ifndef _BANYAN_H_
#define _BANYAN_H_

#include <dlfcn.h>
#include <tcl.h>

#include <boost/algorithm/string.hpp>
#include <boost/process.hpp>
#include <charconv>
#include <cstdint>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

#include "DBAdapter.hpp"
#include "Global.hpp"
#include "clipp.hpp"
#include "magic_enum.hpp"
#include "sta/StaMain.hh"
#include "utl/Logger.h"
#include "Global.hpp"

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

constexpr const std::string_view BANYAN_TMP_FOLDER = "/tmp/banyan/";


struct InstanceData
{
  std::string name;//using inst->GetName()
  int64_t x; //inst center point x
  int64_t y; //inst center point y
};

class Banyan
{
 public:
  Banyan();
  ~Banyan();
  void init(Tcl_Interp* tcl_interp,
            odb::dbDatabase* db,
            stt::SteinerTreeBuilder* stt_builder,
            utl::Logger* logger,
            lrl::Laurel* laurel);

  int main(std::vector<std::string> args);
  int cmap(std::deque<InstanceData> gpInsts, std::string cmapPb);


 protected:
  odb::dbDatabase* db_;
  stt::SteinerTreeBuilder* stt_builder_;
  utl::Logger* logger_;
  lrl::Laurel* laurel_;
}; // class Banyan

}  // namespace banyan

#endif
