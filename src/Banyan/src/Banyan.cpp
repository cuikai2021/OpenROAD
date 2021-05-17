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

  std::vector<banyan_go::net> nets;
  std::vector<banyan_go::pin> pins;
  std::vector<banyan_go::box> boxes;
  std::vector<banyan_go::layer> layers;
  std::vector<banyan_go::track> tracks;

  nets.push_back(banyan_go::net{"NET_1", 0, 2});
  pins.push_back(banyan_go::pin{"PIN_1", 0, 10});
  pins.push_back(banyan_go::pin{"PIN_2", 10, 20});

  for (int i = 0; i < 20; ++i)
      boxes.push_back(banyan_go::box{0, i, i*i, i*i*i, i*i*i*i});

  layers.push_back(banyan_go::layer{100, 5, 0, 1});
  tracks.push_back(banyan_go::track{0, 10, 0, 10000});

  banyan_go::entry_ret ret;

  long long err = banyan(
      banyan_go::to_slice(nets),
      banyan_go::to_slice(pins),
      banyan_go::to_slice(boxes),
      banyan_go::to_slice(layers),
      banyan_go::to_slice(tracks),
      &ret);

  free(ret.rboxes);

  logger_->info(utl::DRT, 666, "banyan returns {}", err);

  return 0;
}

} // namespace Banyan
