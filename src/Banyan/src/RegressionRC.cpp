#include "banyan/Banyan.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <valarray>

#include <eigen3/Eigen/Dense>
#include "ord/OpenRoad.hh"
#include "odb/dbWireCodec.h"
#include "utl/Logger.h"

namespace banyan {

inline namespace {

void assert_(bool expr) {
  if (!expr)
  [[unlikely]]
    ord::OpenRoad::openRoad()->getLogger()
      ->error(utl::BY, 5, "assert failed in regression_RC");
}

void forEachWireLine(odb::dbWire* odbWires,
                 std::function<void(const std::string&, int, int, int, int, int, int)> cb)
{
  auto doAddWire = [&](const std::string& layerName,
                       int x1,
                       int y1,
                       int ext1,
                       int x2,
                       int y2,
                       int ext2) {
    int beginX = x1;
    int beginY = y1;
    int beginExt = ext1;
    int endX = x2;
    int endY = y2;
    int endExt = ext2;
    assert(beginX == endX || beginY == endY);
    if (beginX > endX || beginY > endY) {
      std::swap(beginX, endX);
      std::swap(beginY, endY);
      std::swap(beginExt, endExt);
    }
    cb(layerName, beginX, beginY, beginExt, endX, endY, endExt);
  };

  odb::dbWireDecoder decode;
  int prevX = std::numeric_limits<int>::max();
  int prevY = std::numeric_limits<int>::max();
  int prevExt = std::numeric_limits<int>::max();
  bool hasPrevCoor = false;
  for (decode.begin(odbWires);;) {
    odb::dbWireDecoder::OpCode opcode = decode.next();

    switch (opcode) {
      case odb::dbWireDecoder::PATH:
      case odb::dbWireDecoder::SHORT:
      case odb::dbWireDecoder::VWIRE:
      case odb::dbWireDecoder::JUNCTION: {
        hasPrevCoor = false;
        break;
      }

      case odb::dbWireDecoder::POINT_VIRTUAL: {
        int x, y, ext;
        decode.getPoint(x, y);
        ext = DEFAULT_WIRE_EXT;
        prevX = x;
        prevY = y;
        prevExt = ext;
        hasPrevCoor = true;
        break;
      }

      case odb::dbWireDecoder::POINT: {
        int x, y, ext;
        decode.getPoint(x, y);
        ext = DEFAULT_WIRE_EXT;
        if (hasPrevCoor) {
          doAddWire(decode.getLayer()->getName(), prevX, prevY, prevExt, x, y, ext);
        }
        prevX = x;
        prevY = y;
        prevExt = ext;
        hasPrevCoor = true;
        break;
      }

      case odb::dbWireDecoder::POINT_EXT: {
        int x, y, ext;
        decode.getPoint(x, y, ext);
        if (hasPrevCoor) {
          doAddWire(decode.getLayer()->getName(), prevX, prevY, prevExt, x, y, ext);
        }
        prevX = x;
        prevY = y;
        prevExt = ext;
        hasPrevCoor = true;
        break;
      }

      case odb::dbWireDecoder::VIA: {
        assert(hasPrevCoor);
        //WireVia
        break;
      }

      case odb::dbWireDecoder::TECH_VIA: {
        assert(hasPrevCoor);
        //WireVia
        break;
      }

      case odb::dbWireDecoder::RECT: {
        assert(hasPrevCoor);
        //WireRect
        break;
      }

      case odb::dbWireDecoder::ITERM:
      case odb::dbWireDecoder::BTERM:
      case odb::dbWireDecoder::RULE:
        break;

      case odb::dbWireDecoder::END_DECODE:
        return;

      default:
        break;
    }
  }
}

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
        r_unit = unit * 1e-3;
      else if (buf == "KOHM")
        r_unit = unit * 1;
      else
        ord::OpenRoad::openRoad()->getLogger()->error(
            utl::BY, 5, "unsupported Res unit in spef");
    } else if (s.starts_with("*C_UNIT ")) {
      std::stringstream ss(s);
      std::string buf;
      double unit;
      ss >> buf >> unit >> buf;
      if (buf == "FF")
        c_unit = unit * 1e-3;
      else if (buf == "PF")
        c_unit = unit * 1;
      else
        ord::OpenRoad::openRoad()->getLogger()->error(
            utl::BY, 6, "unsupported Cap unit in spef");
    }
    assert_(s_next());
  }
  if (!r_unit || !c_unit)
    ord::OpenRoad::openRoad()->getLogger()->error(
      utl::BY, 7, "there isnot R/C unit in spef");
  assert_(s_next());

  std::unordered_map<std::string, odb::dbNet*> id2net;
  for (; s != "*PORTS"; assert_(s_next())) {
    std::stringstream ss(s);
    std::string id, name;
    ss >> id >> name;
    assert_(ss.eof());
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
    assert_(ss.eof());

    bool in_res_section = false;
    for (; s != "*END"; assert_(s_next())) {
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
        assert_(ss.eof());
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

    forEachWireLine(dbwire,
                    [&](const std::string& layerName,
                       int beginX,
                       int beginY,
                       int beginExt,
                       int endX,
                       int endY,
                       int endExt) {
                      auto& len = laWireLens[la2id[layerName]];
                      len += endX - beginX + endY - beginY;
                    });

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
  assert_(not _len_mat.empty());
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

void dumpUnitRC2tcl(const std::pair<std::valarray<std::pair<double, double>>,
                                    std::pair<double, double>>& urcs,
                    const char* setRC_tcl,
                    const std::vector<std::string>& route_layers,
                    const std::string& corner_name,
                    const int DB_units_per_micron)
{
  auto fd = std::fopen(setRC_tcl, "a");
  std::fprintf(fd, "\n\n# The following is generated by linear regression:\n");

  std::string ext_args;
  if (!corner_name.empty()) {
    ext_args = " -corner " + corner_name;
  }

  for (size_t la = 0; la < route_layers.size(); la++) {
    auto [ur, uc] = urcs.first[la];
    ur *= DB_units_per_micron;
    uc *= DB_units_per_micron;
    if (ur > 1e-8 && uc > 1e-8)
      std::fprintf(fd, "set_layer_rc -layer %s -capacitance %.11f -resistance %.11f%s\n",
                   route_layers[la].c_str(), uc, ur, ext_args.c_str());
  }
  auto [ur, uc] = urcs.second;
  ur *= DB_units_per_micron;
  uc *= DB_units_per_micron;
  assert_(ur > 1e-8 && uc > 1e-8);
  std::fprintf(fd, "\nset_wire_rc -capacitance %.11f -resistance %.11f%s\n", uc, ur, ext_args.c_str());
  fclose(fd);
}

void dumpPlotPython(const std::vector<std::valarray<int>>& len_mat,
                    const std::vector<std::pair<double, double>>& RC_mat,
                    std::pair<double, double> unit_rc,
                    const std::string& plot_file_prefix
) {
  if (plot_file_prefix.empty())
    return;

  std::stringstream len_p, real_r_v, real_c_v, pred_r_v, pred_c_v;
  auto add_char = [&](char c) {
    len_p << c;
    real_r_v << c;
    real_c_v << c;
    pred_r_v << c;
    pred_c_v << c;
  };
  add_char('[');
  for (size_t i = 0; i<len_mat.size(); i++) {
    auto len = len_mat[i].sum();
    len_p << len;
    real_r_v << RC_mat[i].first;
    real_c_v << RC_mat[i].second;
    pred_r_v << len * unit_rc.first;
    pred_c_v << len * unit_rc.second;
    add_char(',');
  }
  add_char(']');

  auto temp = R"(
import matplotlib.pyplot as plt
lens = %s
real = %s
pred = %s

plt.scatter(lens, real, c='blue', s=2)
plt.scatter(lens, pred, c='cyan', s=2)
plt.xlabel('wire len')
plt.ylabel('%s')
plt.show()
)";

  auto fp_r = fopen((plot_file_prefix + "_r.py").c_str(), "w");
  fprintf(fp_r, temp, len_p.str().c_str(), real_r_v.str().c_str(), pred_r_v.str().c_str(), "resistance");
  fclose(fp_r);

  auto fp_c = fopen((plot_file_prefix + "_c.py").c_str(), "w");
  fprintf(fp_c, temp, len_p.str().c_str(), real_c_v.str().c_str(), pred_c_v.str().c_str(), "capacitor");
  fclose(fp_c);
}

} // inline namespace

void Banyan::regression_RC(const char* spef,
                           const char* setRC_tcl,
                           const char* plot_file_prefix,
                           const char* corner_name)
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
  size_t no_rc_net_cnt = 0;
  for (auto& [net, rc] : RC_data) {
    auto iter = nlwl.find(net);
    if (iter == nlwl.end()) {
      no_rc_net_cnt++;
      continue;
    }
    len_mat.push_back(std::move(iter->second));
    RC_mat.push_back(rc);
  }
  if (no_rc_net_cnt != 0)
    logger_->warn(utl::BY, 8, "there are {} nets which donot have RC data in spef", no_rc_net_cnt);
  auto urcs = rcLinearRegression(len_mat, RC_mat);
  dumpUnitRC2tcl(urcs,
                 setRC_tcl,
                 route_layers,
                 corner_name,
                 db_->getTech()->getDbUnitsPerMicron());
  dumpPlotPython(len_mat, RC_mat, urcs.second, plot_file_prefix);
}

}  // namespace banyan
