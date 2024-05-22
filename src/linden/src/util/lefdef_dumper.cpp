#include "lefdef_dumper.hpp"
#include "odb/db.h"
#include "odb/lefout.h"
#include "ord/OpenRoad.hh"
#include "envflag/envflag.hpp"
#include <chrono>
#include <sstream>
#include <fstream>

using namespace std;
using namespace std::filesystem;

IMPORT_STR_FLAG_WITH_DEFAULT(LINDEN_TMP_FILE_ROOT_DIR);

namespace linden {

namespace {

path write_tech(::ord::OpenRoad& openroad, const path& basename)
{
    path filename = basename;
    filename += ".tech.lef";
    ofstream oss(filename);
    ::odb::lefout out(openroad.getLogger(), oss);
    out.writeTech(openroad.getDb()->getTech());
    return filename;
}

deque<path> write_lib(::ord::OpenRoad& openroad, const path& basename)
{
    const auto& libs = openroad.getDb()->getLibs();
    deque<path> res;
    for (const auto& lib : libs) {
        path filename = basename;
        filename += ".";
        filename += lib->getName();
        filename += ".lib.lef";
        ofstream oss(filename);
        ::odb::lefout out(openroad.getLogger(), oss);
        out.writeLib(const_cast<::odb::dbLib*>(lib));
        res.push_back(std::move(filename));
    }
    return res;
}

path write_def(::ord::OpenRoad& openroad, const path& basename)
{
  path filename = basename;
  filename += ".def";
  openroad.writeDef(filename.c_str(), "5.8");
  return filename;
}

}  // namespace

path gen_base_filename()
{
    auto tm = chrono::system_clock::now().time_since_epoch();
    auto tm_ms = chrono::duration_cast<chrono::milliseconds>(tm).count();
    ostringstream oss;
    oss << LINDEN_TMP_FILE_ROOT_DIR() << "/lindendp_" << tm_ms;
    return oss.str();
}

LefDefDumper::LefDefDumper(::ord::OpenRoad& ord)
{
    basename = gen_base_filename();
    tech_lef = write_tech(ord, basename);
    lib_lefs = write_lib(ord, basename);
    def = write_def(ord, basename);
}

std::ostream& LefDefDumper::print_dumped_files(std::ostream& oss) const
{
    oss << "tech_lef: " << tech_lef << endl;
    for (auto const& liblef : lib_lefs) {
        oss << "lib_lef: " << liblef << endl;
    }
    oss << "in def: " << def << endl;
    return oss;
}

}  // namespace linden
