#pragma once

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/adapted/boost_polygon.hpp>
#include <fstream>
#include <utility>
#include <vector>

#include "ByAdapter.hpp"
#include "ByBox.hpp"
#include "odb/ZException.h"
#include "odb/db.h"
#include "odb/dbShape.h"
#include "odb/dbTransform.h"
#include "odb/dbWireCodec.h"
#include "odb/geom.h"
#include "utl/Logger.h"

namespace banyan {

class PinCluster
{
 public:
  PinCluster(const ByBox& bbox, const std::vector<ByBox>& boxes)
      : bbox_(bbox), boxes_(boxes)
  {
  }

 public:
  const ByBox& bbox() const { return bbox_; }
  std::vector<ByBox>& boxes() { return boxes_; }
  const std::vector<ByBox>& boxes() const { return boxes_; }

 private:
  ByBox bbox_;
  std::vector<ByBox> boxes_;
};

class PinClusterBuilder
{
 public:
  PinClusterBuilder(odb::dbDatabase* db,
            const std::shared_ptr<ByDb>& bydb,
            utl::Logger* logger)
      : db_(db), bydb_(bydb), logger_(logger)
  {
  }

  std::vector<std::vector<PinCluster>> getPinClusters();

 private:
  std::vector<PinCluster> getPinClusters(
      const std::shared_ptr<ByBox>& row,
      const std::vector<std::shared_ptr<ByBox>>& boxes);

 private:
  odb::dbDatabase* db_;
  std::shared_ptr<ByDb> bydb_;
  utl::Logger* logger_;
};

}  // namespace banyan
