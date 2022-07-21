#ifndef _BANYAN_H_
#define _BANYAN_H_

#include <dlfcn.h>
#include <tcl.h>

#include <boost/algorithm/string.hpp>
#include <charconv>
#include <cstdint>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "DBAdapter.hpp"
#include "Global.hpp"
#include "axioms/Axioms.hpp"
#include "banyan_go.h"
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

using namespace clipp;

enum class BanyanCMD
{
  RUN,
  HELP
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
  int run(OpenDBAdapter& bdAdapter, Axioms& axioms);


 protected:
  odb::dbDatabase* db_;
  stt::SteinerTreeBuilder* stt_builder_;
  utl::Logger* logger_;
  lrl::Laurel* laurel_;
}; // class Banyan

}  // namespace banyan

#endif
