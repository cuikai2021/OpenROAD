#include "banyan/Banyan.h"

#include <dlfcn.h>

#include "DBAdapter.h"
#include "axioms/Axioms.h"
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
                  stt::SteinerTreeBuilder* stt_builder,
                  utl::Logger* logger,
                  lrl::Laurel* laurel)
{
  db_ = db;
  stt_builder_ = stt_builder;
  logger_ = logger;
  laurel_ = laurel;
  // Define swig TCL commands.
  Banyan_Init(tcl_interp);
  sta::evalTclInit(tcl_interp, sta::Banyan_tcl_inits);
}

int Banyan::main(const std::map<std::string, std::string>& options)
{
  void* so = dlopen("banyan.so", RTLD_NOW);
  if (so == nullptr) {
    logger_->error(
        utl::BY, 500, "cannot load banyan.so: Have you set LD_LIBRARY_PATH?");
    return -1;
  }

  auto banyan_version = (banyan_go::version_func) dlsym(so, "banyan_version");
  if (banyan_version == nullptr) {
    logger_->error(utl::BY, 501, "cannot find banyan_version in banyan.so");
    return -1;
  }
  char interface_version[100];
  sprintf(interface_version,
          "%02x%02x%02x%02x-%02x%02x%02x%02x",
          banyan_go::interface_version_0,
          banyan_go::interface_version_1,
          banyan_go::interface_version_2,
          banyan_go::interface_version_3,
          banyan_go::interface_version_4,
          banyan_go::interface_version_5,
          banyan_go::interface_version_6,
          banyan_go::interface_version_7);
  if (banyan_version(interface_version) != 0) {
    logger_->error(
        utl::BY, 502, "banyan version check failed with {}", interface_version);
    return -1;
  }

  auto banyan_route = (banyan_go::route_func) dlsym(so, "banyan_route");
  if (banyan_route == nullptr) {
    logger_->error(utl::BY, 503, "cannot find banyan_route in banyan.so");
    return -1;
  }

  bool timingDriven = false;
  int maxViaStack = 100000;
  std::string minLayer;
  std::string maxLayer;
  std::vector<banyan_go::option> options_vec;
  for (auto& kv : options) {
    logger_->info(
        utl::BY, 200, "banyan route option: {}={}", kv.first, kv.second);

    if (kv.first == "max_via_stack") {
      if (!kv.second.empty()) {
        maxViaStack = atoi(kv.second.c_str());
      }
      continue;
    }

    if (kv.first == "min_layer") {
      minLayer = kv.second;
      continue;
    }

    if (kv.first == "max_layer") {
      maxLayer = kv.second;
      continue;
    }

    if (kv.first == "timing_driven") {
      timingDriven = kv.second == "true";
    }
    options_vec.push_back({kv.first.c_str(), kv.second.c_str()});
  }

  OpenDBAdapter bdAdapter(db_,
                          stt_builder_,
                          logger_,
                          laurel_,
                          maxViaStack,
                          minLayer,
                          maxLayer,
                          timingDriven);

  Axioms axioms(bdAdapter);

  banyan_go::route_ret ret;
  ::memset(&ret, 0, sizeof(ret));
  long long err = banyan_route(banyan_go::to_slice(options_vec),
                               bdAdapter.getDesign(),
                               banyan_go::to_slice(bdAdapter.getLayers()),
                               banyan_go::to_slice(bdAdapter.getTracks()),
                               banyan_go::to_slice(bdAdapter.getNets()),
                               banyan_go::to_slice(bdAdapter.getPins()),
                               banyan_go::to_slice(bdAdapter.getBoxes()),
                               banyan_go::to_slice(bdAdapter.getVias()),
                               banyan_go::to_slice(bdAdapter.getViadefs()),
                               banyan_go::to_slice(bdAdapter.getTableTriples()),
                               &ret);

  if (err == 0) {
    bdAdapter.updateDB(bdAdapter.getLayers(),
                       bdAdapter.getNets(),
                       bdAdapter.getViadefs(),
                       ret);
    if (ret.rboxes_size != 0)
      free(ret.rboxes);
    if (ret.rvias_size != 0)
      free(ret.rvias);
    logger_->info(utl::BY, 202, "banyan done");
  } else {
    logger_->info(utl::BY, 504, "banyan error: {}", err);
  }

  return err;
}

}  // namespace banyan
