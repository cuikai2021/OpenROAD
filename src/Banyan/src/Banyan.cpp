#include "banyan/Banyan.h"

#include <dlfcn.h>

#include "sta/StaMain.hh"
#include "utl/Logger.h"
#include "banyan_go.h"

namespace sta {
// Tcl files encoded into strings.
extern const char* Banyan_tcl_inits[];
}  // namespace sta

extern "C" {
extern int Banyan_Init(Tcl_Interp* interp);
}

namespace banyan {

Banyan::Banyan()
{
}

Banyan::~Banyan()
{
}

void Banyan::init(Tcl_Interp* tcl_interp,
  odb::dbDatabase* db, utl::Logger* logger)
{
  db_ = db;
  logger_ = logger;
  // Define swig TCL commands.
  Banyan_Init(tcl_interp);
  sta::evalTclInit(tcl_interp, sta::Banyan_tcl_inits);
}

int Banyan::main()
{
  void *so = dlopen("banyan.so", RTLD_NOW);
  if (so == nullptr)
  {
    logger_->error(
      utl::DRT, 667, "cannot load banyan.so: Have you set LD_LIBRARY_PATH?");
      return -1;
  }
  auto banyan = (banyan_go::entry_func)dlsym(so, "banyan");
  if (banyan == nullptr)
  {
    logger_->error(
      utl::DRT, 668, "caonnot find banyan in banyan.so");
      return -1;
  }

  std::vector<banyan_go::box> boxes;
  for (int i = 0; i < 100; ++i)
      boxes.push_back(banyan_go::box{i, i*i, i*i*i, i*i*i*i});

  logger_->info(  
    utl::DRT, 666, "banyan returns {}", banyan(banyan_go::to_slice(boxes)));

  return 0;
}

} // namespace Banyan
