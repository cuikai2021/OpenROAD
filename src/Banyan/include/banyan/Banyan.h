#ifndef _BANYAN_H_
#define _BANYAN_H_

#include <tcl.h>

#include <memory>
#include <string>

namespace odb {
class dbDatabase;
}

namespace utl {
class Logger;
}

const int BY_CMD_ROUTE = 1;
const int BY_CMD_SAVE_TXT = 2;

namespace banyan {

class Banyan
{
 public:
  Banyan();
  ~Banyan();
  void init(Tcl_Interp* tcl_interp, odb::dbDatabase* db, utl::Logger* logger);
  int main(int cmd);

 protected:
  odb::dbDatabase* db_;
  utl::Logger* logger_;
}; // class Banyan

} // namespace banyan

#endif
