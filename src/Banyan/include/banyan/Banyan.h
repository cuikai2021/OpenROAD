#ifndef _BANYAN_H_
#define _BANYAN_H_

#include <dlfcn.h>
#include <tcl.h>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/process.hpp>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/util/message_differencer.h>

#include "banyan_db.pb.h"
#include "ord/OpenRoad.hh"
#include "odb/db.h"

namespace odb {
class dbDatabase;
}

namespace utl {
class Logger;
}

namespace lrl {
class Laurel;
}

namespace stt {
class SteinerTreeBuilder;
}

namespace banyan {
using byvia = std::tuple<int, int, std::string>;

constexpr const std::string_view BANYAN_TMP_FOLDER = "/tmp/banyan/";
class Banyan : public ord::OpenRoadObserver
{
 public:
  Banyan();
  ~Banyan();
  void init(Tcl_Interp* tcl_interp,
            odb::dbDatabase* db,
            stt::SteinerTreeBuilder* stt_builder,
            utl::Logger* logger,
            lrl::Laurel* laurel);

  int main(std::vector<std::string> args, bool updateDB);
  int cmap(std::string_view cmapPb, bool cgr_mode, std::string_view max_layer);
  void clearWires(bool excludeClock, bool excludeSignal);
  void regression_RC(const char* spef,
                     const char* setRC_tcl,
                     const char* plot_file_prefix,
                     const char* corner_name);
  void savedb(std::string dbfile);
  void loaddb(std::string dbfile);
  void loaddef(std::string_view deffile);
  void diffdefs(std::string_view deffileA,std::string_view deffileB, bool exceptWires);
  // From ord::OpenRoad::Observer
  virtual void postReadLef(const char* filename,
                           odb::dbTech*,
                           odb::dbLib*,
                           bool make_tech) override
  {
    if (make_tech) {
      tech_lef_file_name_ = filename;
    } else {
      other_lef_file_names_.emplace_back(filename);
    }
  }

 protected:
  odb::dbDatabase* db_;
  stt::SteinerTreeBuilder* stt_builder_;
  utl::Logger* logger_;
  lrl::Laurel* laurel_;
  std::string tech_lef_file_name_;
  std::vector<std::string> other_lef_file_names_;

 private:
  odb::dbDatabase* createODB(std::string_view deffile);
  std::unique_ptr<Design> createDesign(std::string_view deffile);
};  // class Banyan

}  // namespace banyan

#endif
