
#include "MergeWireSegment.hpp"

#include <assert.h>

#include <algorithm>
#include <deque>
#include <iostream>
#include <map>
#include <unordered_map>

namespace banyan {

MergeWireSegment::netWireMap MergeWireSegment::GetWireFromDb()
{
  logger_->info(
      utl::BY, 1, "--------------- GetWireFromDb begin ------------------");

  netWireMap ans;
  std::string curNet;
  odb::dbBlock* block = db_->getChip()->getBlock();

  auto addLine
      = [&](const std::string& layerName, int x1, int y1, int x2, int y2) {
          MwsLine line(MwsPoint(x1, y1), MwsPoint(x2, y2));
          MwsWire wire(layerName, line);
          ans[curNet].emplace_back(wire);
        };

  auto addRect = [&](const std::string& layerName,
                     int x,
                     int y,
                     int deltaX1,
                     int deltaY1,
                     int deltaX2,
                     int deltaY2) {
    int x1 = x + deltaX1;
    int y1 = y + deltaY1;
    int x2 = x + deltaX2;
    int y2 = y + deltaY2;
    MwsRect rect(MwsPoint(x, y), x1, y1, x2, y2);
    MwsWire wire(layerName, rect);
    ans[curNet].emplace_back(wire);
  };

  auto addVia = [&](const std::string& layerName,
                    const std::string& viaName,
                    int x,
                    int y) {
    MwsVia via(viaName, MwsPoint(x, y));
    MwsWire wire(layerName, via);
    ans[curNet].emplace_back(wire);
  };
  for (auto dbNet : block->getNets()) {
    if (dbNet->isSpecial()) {
      continue;
    }
    auto odbWires = dbNet->getWire();
    if (odbWires != nullptr) {
      curNet = dbNet->getName();
      odb::dbWireDecoder decode;
      int prevX = std::numeric_limits<int>::max();
      int prevY = std::numeric_limits<int>::max();
      bool hasPrevCoor = false;
      bool bkFlag = false;
      for (decode.begin(odbWires); !bkFlag;) {
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
            int x, y;
            decode.getPoint(x, y);
            prevX = x;
            prevY = y;
            hasPrevCoor = true;
            break;
          }

          case odb::dbWireDecoder::POINT: {
            int x, y;
            decode.getPoint(x, y);
            if (hasPrevCoor) {
              addLine(decode.getLayer()->getName(), prevX, prevY, x, y);
            }
            prevX = x;
            prevY = y;
            hasPrevCoor = true;
            break;
          }

          case odb::dbWireDecoder::POINT_EXT: {
            int x, y, ext;
            decode.getPoint(x, y, ext);
            if (hasPrevCoor) {
              addLine(decode.getLayer()->getName(), prevX, prevY, x, y);
            }
            prevX = x;
            prevY = y;
            hasPrevCoor = true;
            break;
          }

          case odb::dbWireDecoder::VIA: {
            assert(hasPrevCoor);
            addVia(decode.getLayer()->getName(),
                   decode.getVia()->getName(),
                   prevX,
                   prevY);
            break;
          }

          case odb::dbWireDecoder::TECH_VIA: {
            assert(hasPrevCoor);
            addVia(decode.getLayer()->getName(),
                   decode.getTechVia()->getName(),
                   prevX,
                   prevY);
            break;
          }

          case odb::dbWireDecoder::RECT: {
            assert(hasPrevCoor);

            int deltaX1;
            int deltaY1;
            int deltaX2;
            int deltaY2;
            decode.getRect(deltaX1, deltaY1, deltaX2, deltaY2);
            addRect(decode.getLayer()->getName(),
                    prevX,
                    prevY,
                    deltaX1,
                    deltaY1,
                    deltaX2,
                    deltaY2);
            break;
          }

          case odb::dbWireDecoder::ITERM:
          case odb::dbWireDecoder::BTERM:
          case odb::dbWireDecoder::RULE:
            break;

          case odb::dbWireDecoder::END_DECODE: {
            bkFlag = true;
            break;
          }

          default:
            break;
        }
      }
    }
  }
  logger_->info(
      utl::BY, 1, "--------------- GetWireFromDb end ------------------");

  return ans;
}

std::deque<MwsLine> MergeWireSegment::mergeLines(std::deque<MwsLine> lines,
                                                 bool isLevel)
{
  std::deque<MwsLine> ans;

  auto cmp = [](MwsLine lhs, MwsLine rhs) {
    if (lhs.start().y() == rhs.start().y()) {
      if (lhs.start().x() == rhs.start().x()) {
        return lhs.end().x() < rhs.end().x();
      }
      return lhs.start().x() < rhs.start().x();
    }
    return lhs.start().y() < rhs.start().y();
  };

  if (!isLevel) {
    for (auto& x : lines) {
      x.route();
    }
  }

  std::sort(lines.begin(), lines.end(), cmp);

  for (int i = 0; i < lines.size(); i++) {
    int j = i;
    while (j < lines.size() && lines[j].start().y() == lines[i].start().y()) {
      ++j;
    }
    int y = lines[i].start().y();
    int lx = lines[i].start().x();
    int rx = lines[i].end().x();
    int mCnt = 1;
    for (int k = i + 1; k < j; k++) {
      if (lines[k].start().x() <= rx) {
        ++mCnt;
        rx = std::max(rx, lines[k].end().x());
      } else {
        ans.emplace_back(MwsPoint(lx, y), MwsPoint(rx, y), mCnt > 1);
        lx = lines[k].start().x();
        rx = lines[k].end().x();
        mCnt = 1;
      }
    }
    ans.emplace_back(MwsPoint(lx, y), MwsPoint(rx, y), mCnt > 1);
    i = j - 1;
  }

  if (!isLevel) {
    for (auto& x : ans) {
      x.route();
    }
  }

  return ans;
}

std::deque<MwsWire> MergeWireSegment::mergeWireLines(
    const std::deque<MwsWire>& wires)
{
  std::deque<MwsWire> ans;
  std::unordered_map<std::string, std::deque<std::deque<MwsLine>>> mp;
  for (auto wire : wires) {
    if (wire.isLine()) {
      auto line = wire.line();
      mp[wire.layerName()].resize(2);
      if (line.isLevel()) {
        mp[wire.layerName()][0].emplace_back(line);
      } else {
        mp[wire.layerName()][1].emplace_back(line);
      }
    }
  }
  for (auto x : wires) {
    if (x.isLine()) {
      continue;
    }
    ans.emplace_back(x);
  }
  for (auto [layer, ddq] : mp) {
    auto l0 = mergeLines(ddq[0], true);
    auto verLines = ddq[1];
    for (auto x : l0) {
      if (x.isPoint() && !x.isMerged()) {
        verLines.emplace_back(x);
      } else {
        ans.emplace_back(layer, x);
      }
    }
    auto l1 = mergeLines(verLines, false);
    for (auto x : l1) {
      ans.emplace_back(layer, x);
    }
  }
  return ans;
}

void f()
{
  int w = 2;
}

void MergeWireSegment::updateDB(netWireMap& result)
{
  logger_->info(
      utl::BY, 1, "--------------- updateDB begin ------------------");

  odb::dbBlock* block = db_->getChip()->getBlock();
  odb::dbTech* tech = db_->getTech();
  odb::dbWireEncoder _wire_encoder;

  for (auto& [net, wires] : result) {
    auto netInOpenDB = block->findNet(net.c_str());
    if (netInOpenDB == nullptr) {
      continue;
    }
    auto wire = odb::dbWire::create(netInOpenDB);
    if (wire == nullptr) {
      wire = netInOpenDB->getWire();
    }
    odb::dbWire::destroy(wire);
  }

  for (auto& [net, wires] : result) {
    auto netInOpenDB = block->findNet(net.c_str());
    if (netInOpenDB == nullptr) {
      continue;
    }
    auto wire = odb::dbWire::create(netInOpenDB);
    if (wire == nullptr) {
      wire = netInOpenDB->getWire();
    }
    _wire_encoder.begin(wire);

    for (auto pbWire : wires) {
      auto layerInOpenDB = tech->findLayer(pbWire.layerName().c_str());
      _wire_encoder.newPath(layerInOpenDB, odb::dbWireType("ROUTED"));
      if (pbWire.isLine()) {
        auto line = pbWire.line();
        auto start = line.start();
        auto end = line.end();
        _wire_encoder.addPoint(start.x(), start.y());
        _wire_encoder.addPoint(end.x(), end.y());
      } else if (pbWire.isRect()) {
        auto rect = pbWire.rect();
        auto point = rect.point();
        _wire_encoder.addPoint(point.x(), point.y());
        _wire_encoder.addRect(rect.xMin(),
                              rect.yMin(),
                              (rect.xMax() - rect.xMin()),
                              (rect.yMax() - rect.yMin()));
      } else if (pbWire.isVia()) {
        auto via = pbWire.via();
        auto point = via.point();
        _wire_encoder.addPoint(point.x(), point.y());
        auto name = via.name();
        odb::dbTechVia* tech_via = tech->findVia(name.c_str());
        if (tech_via != nullptr) {
          _wire_encoder.addTechVia(tech_via);
        } else {
          odb::dbVia* db_via = block->findVia(name.c_str());
          _wire_encoder.addVia(db_via);
        }
      }
    }
    _wire_encoder.end();
  }
  logger_->info(utl::BY, 1, "--------------- updateDB done ------------------");
}

void MergeWireSegment::Run()
{
  logger_->info(utl::BY, 1, "--------------- run begin ------------------");
  auto netWire = GetWireFromDb();
  auto oriNetWire = netWire;
  int before = 0;
  int after = 0;
  for (auto& [net, wires] : netWire) {
    before += wires.size();
    wires = mergeWireLines(wires);
    after += wires.size();
  }
  logger_->info(utl::BY, 1, "merge result: before:{}  after:{}", before, after);
  updateDB(netWire);
  logger_->info(utl::BY, 1, "--------------- run done ------------------");
}

void MergeWireSegment::Test()
{
  logger_->info(utl::BY, 1, "--------------- Test begin ------------------");
  auto netWire = GetWireFromDb();
  auto oriNetWire = netWire;
  int before = 0;
  int after = 0;
  for (auto& [net, wires] : netWire) {
    before += wires.size();
    wires = mergeWireLines(wires);
    after += wires.size();
  }
  logger_->info(utl::BY, 1, "merge result: before:{}  after:{}", before, after);
  testNetWireEqual(oriNetWire, netWire);
  logger_->info(utl::BY, 1, "--------------- Test done ------------------");
}

bool MergeWireSegment::testWireEqual(std::deque<MwsWire>& origin,
                                     std::deque<MwsWire>& result)
{
  auto getMap = [](std::deque<MwsWire>& wires) {
    std::unordered_map<std::string, std::deque<std::deque<MwsLine>>> mp;
    for (auto wire : wires) {
      if (wire.isLine()) {
        auto line = wire.line();
        mp[wire.layerName()].resize(2);
        if (line.isLevel()) {
          mp[wire.layerName()][0].emplace_back(line);
        } else {
          mp[wire.layerName()][1].emplace_back(line);
        }
      }
    }
    return mp;
  };

  auto getLineRect = [](MwsLine line) {
    polygon poly;
    int x1 = line.start().x();
    int x2 = line.end().x();
    int y1 = line.start().y();
    int y2 = y1 + 2;

    if (!line.isLevel()) {
      x2 = x1 + 2;
      y2 = line.end().y();
    }

    point pts[5] = {boost::polygon::construct<point>(x1, y1),
                    boost::polygon::construct<point>(x1, y2),
                    boost::polygon::construct<point>(x2, y2),
                    boost::polygon::construct<point>(x2, y1),
                    boost::polygon::construct<point>(x1, y1)};
    boost::polygon::set_points(poly, pts, pts + 5);
    return poly;
  };

  auto getPolygonSet = [&getLineRect](std::deque<MwsLine>& lines) {
    polygon_set s;
    for (auto& x : lines) {
      auto poly = getLineRect(x);
      s.insert(poly);
    }
    return s;
  };

  auto oMap = getMap(origin);
  auto rMap = getMap(result);
  for (auto& [layer, line] : oMap) {
    for (int i = 0; i <= 1; i++) {
      auto rLine = rMap[layer];
      auto os = getPolygonSet(line[i]);
      auto rs = getPolygonSet(rLine[i]);
      auto w = boost::polygon::equivalence<polygon_set, polygon_set>(os, rs);
      assert(w);
    }
  }
  return true;
}

bool MergeWireSegment::testNetWireEqual(netWireMap& origin, netWireMap& result)
{
  logger_->info(
      utl::BY, 1, "--------------- testNetWireEqual begin ------------------");
  int count = 0;
  for (auto& [net, wires] : origin) {
    count += testWireEqual(wires, result[net]);
  }
  logger_->info(utl::BY, 1, "testcases:{}   passed:{}", origin.size(), count);
  if (origin.size() == count) {
    logger_->info(utl::BY, 1, "!!!!!!!!!!!   all passed !!!!!!!!!!!!");
  } else {
    logger_->info(utl::BY, 1, "@@@@@@@@@@@@@   has wrong @@@@@@@@@@@@@ ");
  }

  logger_->info(
      utl::BY, 1, "--------------- testNetWireEqual done ------------------");
  return true;
}

}  // namespace banyan