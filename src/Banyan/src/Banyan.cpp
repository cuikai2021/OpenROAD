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
    logger_->info(utl::BY,
                  900,
                  "layer pitch {} width {} track_begin {} track_end {} viadef_begin {} viadef_end {}",
                  layer.pitch,
                  layer.width,
                  layer.track_begin,
                  layer.track_end,
                  layer.viadef_begin,
                  layer.viadef_end);
  }

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
                  "viadef name {} top.la {} top.x{} top.y {} top.w {} top.h {} bot.la {} bot.x {} bot.y {} bot.w {} bot.h {}",
                  viadef.name,
                  viadef.top.la,
                  viadef.top.x,
                  viadef.top.y,
                  viadef.top.w,
                  viadef.top.h,
                  viadef.top.la,
                  viadef.bottom.la,
                  viadef.bottom.x,
                  viadef.bottom.y,
                  viadef.bottom.w,
                  viadef.bottom.h);
  }

  for (auto net : bdAdapter.getNets()) {
    logger_->info(
        utl::BY, 900, "net name {} pin_end {} pin_begin {}", net.name, net.pin_begin, net.pin_end);
  }

  for (auto pin : bdAdapter.getPins()) {
    logger_->info(utl::BY,
                  900,
                  "pin name {} box_end {} box_begin {} via_begin {} via_end {}",
                  pin.name,
                  pin.box_begin,
                  pin.box_end,
                  pin.via_begin,
                  pin.via_end);
  }
  // void *so = dlopen("banyan.so", RTLD_NOW);
  // if (so == nullptr)
  // {
  //   logger_->error(
  //     utl::DRT, 667, "cannot load banyan.so: Have you set LD_LIBRARY_PATH?");
  //     return -1;
  // }
  // auto banyan = (banyan_go::entry_func)dlsym(so, "banyan");
  // if (banyan == nullptr)
  // {
  //   logger_->error(
  //     utl::DRT, 668, "caonnot find banyan in banyan.so");
  //     return -1;
  // }

  // std::vector<banyan_go::net> nets;
  // std::vector<banyan_go::pin> pins;
  // std::vector<banyan_go::box> boxes;
  // std::vector<banyan_go::via> vias;
  // std::vector<banyan_go::layer> layers;
  // std::vector<banyan_go::viadef> viadefs;
  // std::vector<banyan_go::track> tracks;

  // nets.push_back(banyan_go::net{"NET_1", 0, 2});
  // pins.push_back(banyan_go::pin{"PIN_1", 0, 10, 0, 5});
  // pins.push_back(banyan_go::pin{"PIN_2", 10, 20, 5, 10});

  // for (int i = 0; i < 20; ++i)
  //   boxes.push_back(banyan_go::box{0, i, i*i, i*i*i, i*i*i*i});
  // for (int i = 0; i < 10; ++i)
  //   vias.push_back(banyan_go::via{i%2, i, i*i});

  // layers.push_back(banyan_go::layer{100, 5, 0, 1, 0, 0});
  // layers.push_back(banyan_go::layer{100, 5, 1, 2, 0, 1});
  // layers.push_back(banyan_go::layer{100, 5, 2, 3, 1, 2});
  // viadefs.push_back(banyan_go::viadef{"VIA_1",
  //   {1, 0, 0, 100, 100}, {0, 0, 0, 200, 200}});
  // viadefs.push_back(banyan_go::viadef{"VIA_2",
  //   {2, 0, 0, 300, 300}, {1, 0, 0, 400, 400}});
  // tracks.push_back(banyan_go::track{0, 10, 0, 10000});
  // tracks.push_back(banyan_go::track{1, 100, 0, 10000});
  // tracks.push_back(banyan_go::track{2, 1000, 0, 10000});

  // banyan_go::entry_ret ret;

  // long long err = banyan(
  //   banyan_go::to_slice(nets),
  //   banyan_go::to_slice(pins),
  //   banyan_go::to_slice(boxes),
  //   banyan_go::to_slice(vias),
  //   banyan_go::to_slice(layers),
  //   banyan_go::to_slice(viadefs),
  //   banyan_go::to_slice(tracks),
  //   &ret);

  // free(ret.rboxes);
  // free(ret.rvias);

  // logger_->info(utl::DRT, 666, "banyan returns {}", err);

  return 0;
}

}  // namespace banyan
