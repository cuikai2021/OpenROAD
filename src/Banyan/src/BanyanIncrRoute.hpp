#include "banyan/Banyan.h"


namespace banyan {
class BanyanIncrRoute {
 public:
  BanyanIncrRoute(Banyan* banyan, std::string max_layer, std::string work_dir, std::string init_db);
  int incrementalRoute(bool do_route_eco=true);

 private:
  void BanyanMain(std::initializer_list<std::string_view> args, bool update=true);
 protected:
  std::string max_layer;

 private:
  std::string last_history;
  std::string work_dir;
  Banyan* banyan;

  // TODO save incr_$round.db
  // int round{0};
  const std::string last_history_db_name{"history.db"};
  std::string last_history_db_path;
  const std::array<std::string, 2> log_level{"--log_level", "debug"};
};
};