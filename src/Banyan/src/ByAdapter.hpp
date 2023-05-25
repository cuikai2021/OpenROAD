#pragma once

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/adapted/boost_polygon.hpp>
#include <fstream>
#include <utility>
#include <vector>

#include "ByDb.hpp"
#include "DebugDefWriter.hpp"
#include "odb/ZException.h"
#include "odb/db.h"
#include "odb/dbShape.h"
#include "odb/dbTransform.h"
#include "odb/dbWireCodec.h"
#include "odb/geom.h"
#include "thread_pool/ThreadPool.h"
#include "utl/Logger.h"
namespace banyan {

class ByAdapter
{
 public:
  ByAdapter(odb::dbDatabase* db, utl::Logger* logger) : db_(db), logger_(logger)
  {
  }
  std::shared_ptr<ByDb> getByDb();

  static odb::dbTechLayer* findFirstRoutingLayer(odb::dbTech* dbTech);

  static odb::dbTechLayer* findNextCutLayer(odb::dbTech* dbTech,odb::dbTechLayer* curLayer);

  static odb::dbTechLayer* findNextRoutingLayer(odb::dbTech* dbTech,odb::dbTechLayer* curLayer);

  static void updateWire(odb::dbDatabase* db,const std::shared_ptr<ByDb>& bydb);

  static int getEndOfLineSpacing(odb::dbTechLayer* layer);

 private:
  void getWireFromDb(
      std::unordered_map<std::string, std::shared_ptr<ByNet>>& netMap,
      std::unordered_map<std::string, std::shared_ptr<ByLayer>>& layerMap,
      std::unordered_map<std::string, std::shared_ptr<ByVia>>& viaMap);

 private:
  odb::dbDatabase* db_;
  utl::Logger* logger_;
};

}  // namespace banyan