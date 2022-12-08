#include "gpl/lindengp.h"

#include <iostream>

#include "banyan/Banyan.h"
#include "banyan_cmap.pb.h"
#include "fassert.hpp"
#include "odb/db.h"

using namespace std;

namespace gpl {

void LindenGlobalPlace::test_baby_banyan()
{
  {  // try use bb
    const std::string cmap_pb_file = [] {
      const std::string basename = [] {
        auto tm = std::chrono::system_clock::now().time_since_epoch();
        auto tm_ms
            = std::chrono::duration_cast<std::chrono::milliseconds>(tm).count();
        std::ostringstream oss;
        oss << "/tmp/banyan_" << tm_ms;
        return oss.str();
      }();
      return basename + ".cmap";
    }();
    auto banyan = ::ord::OpenRoad::openRoad()->getBanyan();
    banyan->cmap(cmap_pb_file);
    banyan::Cmap cmap;
    fstream input(cmap_pb_file, ios::in | ios::binary);
    if (!input) {
      cerr << "Failed to open file:" << cmap_pb_file << endl;
    } else if (!cmap.ParseFromIstream(&input)) {
      cerr << "Failed to parse cmap file" << cmap_pb_file << endl;
      return;
    }
    std::cout << "pb dumped:" << cmap.wgrids() << std::endl;
    for (size_t i = 0; i < cmap.vtiles_size(); ++i) {
      std::cout << "vtile:" << i << ":(" << cmap.vtiles(i).supply() << ", "
                << cmap.vtiles(i).demand() << ")" << std::endl;
    }
  }
}

}  // namespace gpl