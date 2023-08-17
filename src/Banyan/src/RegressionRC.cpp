#include "banyan/Banyan.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <valarray>
#include <vector>

#include <eigen3/Eigen/Dense>
#include "DBAdapter.hpp"
#include "ord/OpenRoad.hh"
#include "utl/Logger.h"

namespace banyan {

inline namespace {

std::unordered_map<odb::dbNet*, std::pair<double, double>> loadRCFromSpef(
    odb::dbBlock* block,
    const char* spef)
{
  std::string s, _;
  std::ifstream fin(spef);
  auto s_next = [&s, &fin]() {
    do {
      if (not std::getline(fin, s))
        return false;
    } while (s.empty());
    return true;
  };

  std::optional<double> r_unit, c_unit;
  while (s != "*NAME_MAP") {
    if (s.starts_with("*R_UNIT ")) {
      std::stringstream ss(s);
      std::string buf;
      double unit;
      ss >> buf >> unit >> buf;
      if (buf == "OHM")
        r_unit = unit * 1;
      else if (buf == "KOHM")
        r_unit = unit * 1000;
      else
        ord::OpenRoad::openRoad()->getLogger()->error(
            utl::BY, 5, "unsupported Res unit in spef");
    } else if (s.starts_with("*C_UNIT ")) {
      std::stringstream ss(s);
      std::string buf;
      double unit;
      ss >> buf >> unit >> buf;
      if (buf == "FF")
        c_unit = unit * 1;
      else if (buf == "PF")
        c_unit = unit * 1000;
      else
        ord::OpenRoad::openRoad()->getLogger()->error(
            utl::BY, 6, "unsupported Cap unit in spef");
    }
    assert(s_next());
  }
  if (!r_unit || !c_unit)
    ord::OpenRoad::openRoad()->getLogger()->error(
      utl::BY, 7, "there isnot R/C unit in spef");
  assert(s_next());

  std::unordered_map<std::string, odb::dbNet*> id2net;
  for (; s != "*PORTS"; assert(s_next())) {
    std::stringstream ss(s);
    std::string id, name;
    ss >> id >> name;
    assert(ss.eof());
    id2net[id] = block->findNet(name.c_str());
  }

  std::unordered_map<odb::dbNet*, std::pair<double, double>> RC_data;
  for (;;) {
    while (!s.starts_with("*D_NET"))
      if (!s_next()) {
        fin.close();
        return RC_data;
      }

    std::stringstream ss(s);
    std::string id;
    ss >> _ >> id;
    auto net = id2net[id];
    auto& RC = RC_data[net];
    ss >> RC.second;
    assert(ss.eof());

    bool in_res_section = false;
    for (; s != "*END"; assert(s_next())) {
      if (s == "*RES")
        in_res_section = true;
      else if (s == "*CONN")
        in_res_section = false;
      else if (s == "*CAP")
        in_res_section = false;
      else if (in_res_section) {
        ss.clear();
        ss.str(s);
        double res;
        ss >> _ >> _ >> _ >> res;
        assert(ss.eof());
        RC.first += res;
      }
    }

    RC.first  *= *r_unit;
    RC.second *= *c_unit;
    if (RC.first == 0)
      RC_data.erase(net);
  }
}

std::unordered_map<odb::dbNet*, std::valarray<int>>
statNetLaWireLens(odb::dbDatabase* db, const std::vector<std::string>& route_layers) {
  std::unordered_map<std::string, int> la2id(route_layers.size());
  for (size_t i = 0; i < route_layers.size(); i++)
    la2id[route_layers[i]] = i;

  std::unordered_map<odb::dbNet*, std::valarray<int>> result;
  for (auto net : db->getChip()->getBlock()->getNets()) {
    auto& laWireLens = result[net] = std::valarray<int>(route_layers.size());

    auto dbwire = net->getWire();
    if (!dbwire) {
      result.erase(net);
      continue;
    }
    for (auto& wire : OpenDBAdapter::odbWires2Wires(dbwire)) {
      auto& len = laWireLens[la2id[wire.layername()]];
      for (auto& seg : wire.segments()) {
        if (seg.has_line()) {
          auto line = seg.line();
          len += line.end().point().x() - line.start().point().x()
              +  line.end().point().y() - line.start().point().y();
        }
      }
    }

    for (auto pin : net->getITerms())
      for (auto mpin : pin->getMTerm()->getMPins())
        for (auto geo : mpin->getGeometry())
          if (!geo->isVia())
            laWireLens[la2id[geo->getTechLayer()->getConstName()]] += std::max(geo->getDX(), geo->getDY());
  }
  return result;
}

std::pair<std::valarray<std::pair<double, double>>, std::pair<double, double>>
rcLinearRegression(std::vector<std::valarray<int>> _len_mat, std::vector<std::pair<double, double>> _rc_mat) {
  assert(not _len_mat.empty());
  auto net_cnt = _len_mat.size();
  auto la_cnt = _len_mat.front().size();

  Eigen::MatrixXd lens_mat(net_cnt, la_cnt);
  Eigen::MatrixXd len_mat(net_cnt, 1);
  Eigen::MatrixXd rc_mat(net_cnt, 2);
  for (size_t i = 0; i < net_cnt; ++i) {
    for (size_t j = 0; j < la_cnt; ++j) {
      lens_mat(i, j) = _len_mat[i][j];
    }
    len_mat(i, 0) = _len_mat[i].sum();
    rc_mat(i, 0) = _rc_mat[i].first;
    rc_mat(i, 1) = _rc_mat[i].second;
  }

  Eigen::MatrixXd urc_la = lens_mat.bdcSvd(Eigen::ComputeThinV|Eigen::ComputeThinU).solve(rc_mat);
  Eigen::MatrixXd urc = len_mat.bdcSvd(Eigen::ComputeThinV|Eigen::ComputeThinU).solve(rc_mat);

  std::valarray<std::pair<double, double>> _urc_la(net_cnt);
  for (size_t i = 0; i < la_cnt; ++i) {
    _urc_la[i] = {urc_la(i, 0), urc_la(i, 1)};
  }
  return {_urc_la, {urc(0, 0), urc(0, 1)}};
}

void dumpUnitRC2tcl(const std::pair<std::valarray<std::pair<double, double>>, std::pair<double, double>>& urcs,
                    const char *setRC_tcl, const std::vector<std::string>& route_layers
) {
  auto fd = std::fopen(setRC_tcl, "a");
  std::fprintf(fd, "\n\n# The following is generated by linear regression:\n");

  for (size_t la = 0; la < route_layers.size(); la++) {
    auto [ur, uc] = urcs.first[la];
    if (ur > 1e-8 && uc > 1e-8)
      std::fprintf(fd, "set_layer_rc -layer %s -capacitance %.11f -resistance %.11f\n",
                   route_layers[la].c_str(), uc, ur);
  }
  auto [ur, uc] = urcs.second;
  assert(ur > 1e-8 && uc > 1e-8);
  std::fprintf(fd, "\nset_wire_rc -capacitance %.11f -resistance %.11f\n", uc, ur);
  fclose(fd);
}

} // inline namespace

void Banyan::regression_RC(const char *spef, const char *setRC_tcl)
{
  // TODO: 区分 clock net / signal net
  //            拥塞区域...
  //       关键路径 net 加权...
  std::vector<std::string> route_layers;
  for (auto la : db_->getTech()->getLayers())
    if (la->getType() == odb::dbTechLayerType::ROUTING)
      route_layers.emplace_back(la->getConstName());

  auto RC_data = loadRCFromSpef(db_->getChip()->getBlock(), spef);
  auto nlwl = statNetLaWireLens(db_, route_layers);
  std::vector<std::valarray<int>> len_mat;
  std::vector<std::pair<double, double>> RC_mat;
  for (auto& [net, rc] : RC_data) {
    auto iter = nlwl.find(net);
    if (iter == nlwl.end())
      continue;
    len_mat.push_back(std::move(iter->second));
    RC_mat.push_back(rc);
  }
  auto urcs = rcLinearRegression(len_mat, RC_mat);
  dumpUnitRC2tcl(urcs, setRC_tcl, route_layers);
}

}  // namespace banyan
