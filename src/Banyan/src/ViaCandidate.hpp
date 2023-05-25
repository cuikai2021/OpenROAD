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

#include "GridlessMaze.hpp"

namespace banyan {

class ViaCandidate
{
 public:
  ViaCandidate(const std::string& netName,
               const std::pair<std::string, std::string>& pinName,
               const std::shared_ptr<ByViaInst>& via,
               const std::vector<ByBox>& wires = {})
      : netName_(netName), pinName_(pinName), via_(via), wires_(wires)
  {
  }

  const std::string& getNetName() const { return netName_; }
  void setNetName(const std::string& netName) { netName_ = netName; }
  const std::pair<std::string, std::string>& getPinName() const
  {
    return pinName_;
  }
  void setPinName(const std::pair<std::string, std::string>& pinName)
  {
    pinName_ = pinName;
  }
  const std::shared_ptr<ByViaInst>& getVia() const { return via_; }
  void setVia(const std::shared_ptr<ByViaInst>& via) { via_ = via; }
  const std::vector<ByBox>& getWires() const { return wires_; }
  void setWires(const std::vector<ByBox>& wires) { wires_ = wires; }

 private:
  std::string netName_;
  std::pair<std::string, std::string> pinName_;
  std::shared_ptr<ByViaInst> via_;
  std::vector<ByBox> wires_;
};

class ViaCandidateBuilder
{
 public:
  ViaCandidateBuilder(odb::dbDatabase* db,
                      const std::shared_ptr<ByDb>& bydb,
                      const std::vector<GmVirtualPin>& vpins,
                      utl::Logger* logger)
      : db_(db), bydb_(bydb), vpins(vpins), logger_(logger)
  {
  }
  std::vector<std::shared_ptr<ViaCandidate>> run();

 private:
  std::vector<std::shared_ptr<ViaCandidate>> solveRow(
      const std::shared_ptr<ByBox>& row,
      const std::vector<std::shared_ptr<ByBox>>& boxes);

  std::vector<std::pair<int, int>> getGridPoint(
      const std::vector<int>& vx,
      const std::vector<int>& vy,
      const std::shared_ptr<ByBox>& b);

  bool checkDRC(const ByBox& a, const ByBox& b);

  bool checkPRL(const ByBox& a, const ByBox& b);

  void getPinCandidate(std::vector<std::shared_ptr<ViaCandidate>>&ans,const ByBox& pin,
                                            const std::shared_ptr<ByViaInst>& via);

 private:
  odb::dbDatabase* db_;
  std::shared_ptr<ByDb> bydb_;
  std::vector<GmVirtualPin>vpins;
  utl::Logger* logger_;
  std::vector<int> gx_, gy_;
  int m1Spacing_;
  odb::dbTechLayer *cut1_, *m1_, *m2_;
};

}  // namespace banyan
