#ifndef _BANYAN_H_
#define _BANYAN_H_

#include <tcl.h>

#include <memory>
#include <string>
#include <map>

namespace odb {
class dbDatabase;
}

namespace utl {
class Logger;
}

namespace lrl {
class Laurel;
}

namespace banyan {

class Banyan
{
 public:
  Banyan();
  ~Banyan();
  void init(Tcl_Interp* tcl_interp, odb::dbDatabase* db, utl::Logger* logger, lrl::Laurel* laurel);
  int main(const std::map<std::string, std::string> &options);

 protected:
  odb::dbDatabase* db_;
  utl::Logger* logger_;
  lrl::Laurel* laurel_;
}; // class Banyan

} // namespace banyan

#endif
