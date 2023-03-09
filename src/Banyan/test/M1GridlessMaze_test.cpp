#include <tcl.h>

#include <filesystem>
#include <tuple>
#include <vector>

#include "GridlessMaze.hpp"
#include "banyan/MakeBanyan.h"
#include "odb/defin.h"
#include "odb/lefin.h"
#include "utl/Logger.h"
#include "utl/MakeLogger.h"
#define mp metaprogramming
#include "testa.hpp"
#undef mp

namespace {

using namespace banyan;
using namespace std;
using namespace std::filesystem;

string getFileName(string s)
{
  auto ans = s;
  size_t found = s.find_last_of('.');
  if (found != std::string::npos) {
    ans = ans.substr(0, found);
  }
  found = ans.find_last_of('/');
  if (found != std::string::npos) {
    ans = ans.erase(0, found + 1);
  }
  return ans;
}

vector<string> getCasesPath()
{
  auto curdir = std::filesystem::current_path().string();
  string dir;
  for (auto& x : curdir) {
    dir += x;
    int n = dir.size();
    if (dir.size() >= 4 && dir[n - 4] == 'E' && dir[n - 3] == 'd'
        && dir[n - 2] == 'e' && dir[n - 1] == 'n') {
      break;
    }
  }
  dir += "/src/Banyan/test/cases";
  std::vector<std::string> ans;
  for (const auto& file : directory_iterator(dir)) {
    if (is_directory(file)) {
      for (const auto& sfile : directory_iterator(file.path())) {
        ans.emplace_back(sfile.path().string());
      }
    }
  }
  return ans;
}

void readLef(odb::dbDatabase* db,
             utl::Logger* logger,
             const char* filename,
             const char* lib_name,
             bool make_tech,
             bool make_library)
{
  odb::lefin lef_reader(db, logger, false);
  odb::dbLib* lib = nullptr;
  odb::dbTech* tech = nullptr;
  if (make_tech && make_library) {
    lib = lef_reader.createTechAndLib(lib_name, filename);
    tech = db->getTech();
  } else if (make_tech) {
    tech = lef_reader.createTech(filename);
  } else if (make_library) {
    lib = lef_reader.createLib(lib_name, filename);
  }
}

void readDef(odb::dbDatabase* db,
             utl::Logger* logger,
             const char* filename,
             bool continue_on_errors,
             bool floorplan_init,
             bool incremental)
{
  odb::defin::MODE mode = odb::defin::DEFAULT;
  if (floorplan_init)
    mode = odb::defin::FLOORPLAN;
  else if (incremental)
    mode = odb::defin::INCREMENTAL;
  odb::defin def_reader(db, logger, mode);
  std::vector<odb::dbLib*> search_libs;
  for (odb::dbLib* lib : db->getLibs())
    search_libs.push_back(lib);
  if (continue_on_errors) {
    def_reader.continueOnErrors();
  }
  odb::dbChip* chip = def_reader.createChip(search_libs, filename);
}

void M1GridlessMazeTestBench(
    const string& name,
    function<void(const tuple<string, odb::dbDatabase*>&)> cs)
{
  auto logger = new utl::Logger();
  auto getDb = [&](string& casePath) {
    odb::dbDatabase* db = nullptr;
    db = odb::dbDatabase::create();
    auto inPath = casePath;
    path inDir(inPath);
    if (!is_directory(inDir)) {
      db = nullptr;
      return db;
    }
    for (const auto& file : directory_iterator(inDir)) {
      auto fileName = file.path().string();
      if (fileName == inPath + "/tech.lef") {
        readLef(db, logger, fileName.c_str(), "", true, false);
      }
    }
    for (const auto& file : directory_iterator(inDir)) {
      auto fileName = file.path().string();
      int n = fileName.size();
      if (n < 3) {
        continue;
      }
      if (fileName == inPath + "/tech.lef") {
        continue;
      } else if (fileName[n - 3] == 'l' && fileName[n - 2] == 'e'
                 && fileName[n - 1] == 'f') {
        readLef(db,
                logger,
                fileName.c_str(),
                getFileName(fileName).c_str(),
                false,
                true);
      }
    }
    for (const auto& file : directory_iterator(inDir)) {
      auto fileName = file.path().string();
      int n = fileName.size();
      if (n < 3) {
        continue;
      }
      if (fileName == inPath + "/tech.lef") {
        continue;
      } else if (fileName[n - 3] == 'd' && fileName[n - 2] == 'e'
                 && fileName[n - 1] == 'f') {
        readDef(db, logger, fileName.c_str(), false, false, false);
      }
    }
    return db;
  };

  auto casesPath = getCasesPath();
  TESTA_ASSERT(!casesPath.empty())
  (std::filesystem::current_path().string()).issue();

  for (auto& p : casesPath) {
    auto db = getDb(p);
    if (db != nullptr) {
      cs({p, db});
      odb::dbDatabase::destroy(db);
    }
  }
}

vector<GmVirtualPin> Run(const tuple<string, odb::dbDatabase*>& in)
{
  auto logger = new utl::Logger();
  auto gm = GridlessMaze(
      get<1>(in), logger, GridlessMaze::SearchLimitLevel::MEDIUM);
  return gm.Run();
}

void M1GridlessMazeVerifier(const vector<GmVirtualPin>& res,
                            const tuple<string, odb::dbDatabase*>& in)
{
  using DPoint
      = boost::geometry::model::d2::point_xy<int,
                                             boost::geometry::cs::cartesian>;
  using DBox = boost::geometry::model::box<DPoint>;
  using Value = std::pair<DBox, unsigned>;
  using RTree = boost::geometry::index::
      rtree<Value, boost::geometry::index::quadratic<100000>>;

  auto getLineTuple = [](const GmRoutLine& w) {
    return std::tuple<int, int, int, int>(w.lx, w.ly, w.rx, w.ry);
  };

  RTree rt;
  for (const auto& vp : res) {
    for (const auto& w : vp.lines) {
      DBox b(DPoint(w.lx, w.ly), DPoint(w.rx, w.ry));
      std::vector<Value> res;
      rt.query(boost::geometry::index::intersects(b), std::back_inserter(res));
      auto intersetion = !res.empty();
      auto info = "path intersection: " + pp::prettyPrint(getLineTuple(w));
      TESTA_ASSERT(!intersetion)(vp.names).issue();
    }
    for (const auto& w : vp.lines) {
      DBox b(DPoint(w.lx, w.ly), DPoint(w.rx, w.ry));
      rt.insert(std::make_pair(b, 0));
    }
  }

  std::unordered_set<std::string> st;
  for (const auto& w : res) {
    for (const auto& s : w.names) {
      if (st.count(s)) {
        TESTA_ASSERT(false)("pin intersection: " + s).issue();
      }
      st.insert(s);
    }
  }
}

TESTA_DEF_VERIFY_WITH_TB(M1GridlessMazeTest,
                         M1GridlessMazeTestBench,
                         M1GridlessMazeVerifier,
                         Run);
}  // namespace