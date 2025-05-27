#include "BanyanIncrRoute.hpp"


namespace banyan {


BanyanIncrRoute::BanyanIncrRoute(Banyan* banyan, std::string max_layer, std::string work_dir, std::string init_db) : 
  max_layer(max_layer), work_dir(work_dir), banyan(banyan) {
  
  if (!init_db.empty()) {
    last_history_db_path = init_db;
    return;
  }
  last_history_db_path = work_dir + "/" + last_history_db_name;
  BanyanMain(
    {
      "load_data",
      "--save_db", last_history_db_path,
    }, 
      false);
}

int
BanyanIncrRoute::incrementalRoute(bool do_route_eco)
{
  BanyanMain(
    {
    "incr_route",
    "--history", last_history_db_path,
    "--max_layer", max_layer,
    }, 
    true);
  last_history_db_path = work_dir + "/" + last_history_db_name;
  if (do_route_eco) {
    // TODO min_layer
    auto min_layer = ord::OpenRoad::openRoad()->getDb()->getTech()->findRoutingLayer(1);
    assert(min_layer != nullptr);
    BanyanMain(
      {
      "refine_route",
      "--fix_drc",
      "--max_layer", max_layer,
      "--min_layer", min_layer->getName(),
      "--save_db", last_history_db_path,
      }, 
      true);
  }
  return 0;
}

void 
BanyanIncrRoute::BanyanMain(std::initializer_list<std::string_view> args, bool update)
{
  std::vector<std::string> args_with_default{args.begin(), args.end()};
  args_with_default.insert(args_with_default.end(), log_level.begin(), log_level.end());
  banyan->main(args_with_default, update);
}

};