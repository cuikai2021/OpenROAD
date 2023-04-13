#include "linden/lefdef_dumper.hpp"

#include <chrono>
#include <sstream>

#include "envflag/envflag.hpp"

#include "odb/db.h"
#include "odb/lefout.h"
#include "ord/OpenRoad.hh"

using namespace std;

IMPORT_STR_FLAG_WITH_DEFAULT(LINDEN_TMP_FILE_ROOT_DIR);

namespace linden {
namespace dp {

namespace {
string genBaseFilename()
{
  auto tm = chrono::system_clock::now().time_since_epoch();
  auto tm_ms = chrono::duration_cast<chrono::milliseconds>(tm).count();
  ostringstream oss;
  oss << LINDEN_TMP_FILE_ROOT_DIR() << "/lindendp_" << tm_ms;
  return oss.str();
}

string writeTech(::ord::OpenRoad& openroad, const string& basename)
{
  string filename = basename;
  filename.append(".tech.lef");
  ::odb::lefout out(openroad.getLogger());
  out.writeTech(openroad.getDb()->getTech(), filename.c_str());
  return filename;
}

deque<string> writeLib(::ord::OpenRoad& openroad, const string& basename)
{
  const auto& libs = openroad.getDb()->getLibs();
  deque<string> res;
  for (const auto& lib : libs) {
    string filename = basename;
    filename.push_back('.');
    filename.append(lib->getName());
    filename.append(".lib.lef");
    ::odb::lefout out(openroad.getLogger());
    out.writeLib(const_cast<::odb::dbLib*>(lib), filename.c_str());
    res.push_back(std::move(filename));
  }
  return res;
}

string writeDef(::ord::OpenRoad& openroad, const string& basename)
{
  string filename = basename;
  filename.append(".def");
  openroad.writeDef(filename.c_str(), "5.8");
  return filename;
}

}  // namespace

void LefDefDumper::run(::ord::OpenRoad& ord)
{
  _basename = std::move(genBaseFilename());
  _tech_lef = std::move(writeTech(ord, _basename));
  _lib_lefs = std::move(writeLib(ord, _basename));
  _def = std::move(writeDef(ord, _basename));
}

std::ostream& LefDefDumper::print_dumped_files(std::ostream& oss) const
{
  oss << "tech_lef: " << _tech_lef << endl;
  for (auto const& liblef : _lib_lefs) {
    oss << "lib_lef: " << liblef << endl;
  }
  oss << "in def: " << _def << endl;
  return oss;
}

}  // namespace dp
}  // namespace linden