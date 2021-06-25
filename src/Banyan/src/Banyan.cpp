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

int Banyan::main()
{
  OpenDBAdapter bdAdapter(db_, logger_);

  for (auto layer : bdAdapter.getLayers()) {
    logger_->info(
        utl::BY,
        900,
        "layer name {} pitch {} width {} tracks [{},{}) viadefs[{},{})",
        layer.name,
        layer.pitch,
        layer.width,
        layer.track_begin,
        layer.track_end,
        layer.viadef_begin,
        layer.viadef_end);
  }

  for (auto viadef : bdAdapter.getViadefs()) {
    logger_->info(
        utl::BY,
        900,
        "viadef name {} top.la {} top.x{} top.y {} top.w {} top.h {} bot.la {} bot.x {} bot.y {} bot.w {} bot.h {} ", 
        viadef.name, 
        viadef.top.la,
        viadef.top.x,
        viadef.top.y,
        viadef.top.w,
        viadef.top.h,
        viadef.bottom.la,
        viadef.bottom.x,
        viadef.bottom.y,
        viadef.bottom.w,
        viadef.bottom.h);
  }
  /*
    for (auto track : bdAdapter.getTracks()) {
      logger_->info(utl::BY,
                    900,
                    "track layer {} line {} tmin {} tmax {}",
                    track.la,
                    track.line,
                    track.tmin,
                    track.tmax);
    }

    for (auto viadef : bdAdapter.getViadefs()) {
      logger_->info(utl::BY,
                    900,
                    "viadef name {} top.la {} top.x{} top.y {} top.w {} top.h {}
    bot.la {} bot.x {} bot.y {} bot.w {} bot.h {}", viadef.name, viadef.top.la,
                    viadef.top.x,
                    viadef.top.y,
                    viadef.top.w,
                    viadef.top.h,
                    viadef.bottom.la,
                    viadef.bottom.x,
                    viadef.bottom.y,
                    viadef.bottom.w,
                    viadef.bottom.h);
    }

    for (auto net : bdAdapter.getNets()) {
      logger_->info(
          utl::BY, 900, "net name {} pin_end {} pin_begin {}", net.name,
    net.pin_begin, net.pin_end);
    }

    for (auto pin : bdAdapter.getPins()) {
      logger_->info(utl::BY,
                    900,
                    "pin name {} box_end {} box_begin {} via_begin {} via_end
    {}", pin.name, pin.box_begin, pin.box_end, pin.via_begin, pin.via_end);
    }

    for (auto box : bdAdapter.getBoxes()) {
      logger_->info(utl::BY,
                    900,
                    "box la {} x {} y {} w {} h {}",
                    box.la,
                    box.x,
                    box.y,
                    box.w,
                    box.h);
    }

    for (auto via : bdAdapter.getVias()) {
      logger_->info(utl::BY,
                    900,
                    "via viadef_index {} x {} y {}",
                    via.viadef_index,
                    via.x,
                    via.y);
    }
  */
  void* so = dlopen("banyan.so", RTLD_NOW);
  if (so == nullptr) {
    logger_->error(
        utl::BY, 667, "cannot load banyan.so: Have you set LD_LIBRARY_PATH?");
    return -1;
  }
  auto banyan = (banyan_go::entry_func) dlsym(so, "banyan");
  if (banyan == nullptr) {
    logger_->error(utl::BY, 668, "caonnot find banyan in banyan.so");
    return -1;
  }

  banyan_go::entry_ret ret;

  long long err = banyan(banyan_go::to_slice(bdAdapter.getNets()),
                         banyan_go::to_slice(bdAdapter.getPins()),
                         banyan_go::to_slice(bdAdapter.getBoxes()),
                         banyan_go::to_slice(bdAdapter.getVias()),
                         banyan_go::to_slice(bdAdapter.getLayers()),
                         banyan_go::to_slice(bdAdapter.getViadefs()),
                         banyan_go::to_slice(bdAdapter.getTracks()),
                         &ret);

  if (ret.rboxes_size != 0)
    free(ret.rboxes);
  if (ret.rvias_size != 0)
    free(ret.rvias);

  logger_->info(utl::BY, 666, "banyan returns {}", err);

  return 0;
}

}  // namespace banyan
