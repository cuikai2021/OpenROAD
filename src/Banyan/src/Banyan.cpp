#include "banyan/Banyan.h"

#include <dlfcn.h>

#include "DBAdapter.h"
#include "banyan_go.h"
#include "sta/StaMain.hh"
#include "utl/Logger.h"

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
                  odb::dbDatabase* db,
                  utl::Logger* logger)
{
  db_ = db;
  logger_ = logger;
  // Define swig TCL commands.
  Banyan_Init(tcl_interp);
  sta::evalTclInit(tcl_interp, sta::Banyan_tcl_inits);
}

std::string byCmdToFuncName(int cmd){
  switch (cmd)
  {
  case BY_CMD_ROUTE:
    return "banyan";

  case BY_CMD_SAVE_TXT:
    return "banyanSaveTXT";
  
  default:
    return "";
  }
}

int Banyan::main(int cmd)
{

  std::string funcName = byCmdToFuncName(cmd);
  logger_->debug(utl::BY, 666, "banyan main {} {}", cmd, funcName);
  
  OpenDBAdapter bdAdapter(db_, logger_);

  for (auto layer : bdAdapter.getLayers()) {
    logger_->info(utl::BY,
                  900,
                  "layer name {} dir {} pitch {} width {} tracks [{},{}) viadefs[{},{})",
                  layer.name,
                  layer.dir,
                  layer.pitch,
                  layer.width,
                  layer.track_begin,
                  layer.track_end,
                  layer.viadef_begin,
                  layer.viadef_end);
  }
  
  void* so = dlopen("banyan.so", RTLD_NOW);
  if (so == nullptr) {
    logger_->error(
        utl::BY, 667, "cannot load banyan.so: Have you set LD_LIBRARY_PATH?");
    return -1;
  }
  auto banyan = (banyan_go::entry_func) dlsym(so, funcName.c_str());
  if (banyan == nullptr) {
    logger_->error(utl::BY, 668, "caonnot find banyan in banyan.so");
    return -1;
  }

  banyan_go::entry_ret ret;

  long long err = banyan(
    banyan_go::to_slice(bdAdapter.getNets()),
    banyan_go::to_slice(bdAdapter.getPins()),
    banyan_go::to_slice(bdAdapter.getBoxes()),
    banyan_go::to_slice(bdAdapter.getVias()),
    banyan_go::to_slice(bdAdapter.getLayers()),
    banyan_go::to_slice(bdAdapter.getViadefs()),
    banyan_go::to_slice(bdAdapter.getTracks()),
    bdAdapter.getDesign(), &ret);

  if (cmd == BY_CMD_ROUTE) {
    bdAdapter.updateDB(bdAdapter.getNets(), ret);
  }


  if (ret.rboxes_size != 0)
    free(ret.rboxes);
  if (ret.rvias_size != 0)
    free(ret.rvias);

  logger_->info(utl::BY, 666, "banyan returns {}", err);

  return 0;
}

}  // namespace banyan
