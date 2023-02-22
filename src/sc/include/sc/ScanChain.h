#pragma once

#include "db.h"
#include "Graph.h"

namespace ord {
class OpenRoad;
}  //namespace ord

namespace utl {
class Logger;
} // namespace utl

namespace odb {
class dbDatabase;
class dbBlock;
class dbInst;
class dbNet;
class dbITerm;
}  // namespace odb

namespace sc {
class ScanChain {
 public:
  ScanChain() = default;
  ~ScanChain();

  ord::OpenRoad* open_road_;
  utl::Logger* logger_;
  odb::dbDatabase* db_;
  odb::dbBlock* block_ = nullptr;

  void init(ord::OpenRoad* openroad);
  void reorder();
  void trace();
  void reportScanChain(const char *filename);

 private:
  bool traceScanChain(const odb::dbScanChain *scan_chain, double& distance);
  void initPoints(const odb::dbScanChain *scan_chain, std::vector<Point> &points);
  std::vector<Graph::NodeId> reorderScanChain(const odb::dbScanChain *scan_chain, const std::vector<Point> &points);
  void connectScanNet(const odb::dbScanChain *scan_chain, const std::vector<Graph::NodeId> &orders);
};
} // namespace sc