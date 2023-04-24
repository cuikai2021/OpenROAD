#define mp metaprogramming
#include "testa.hpp"
#undef mp
#include <random>
#include <string>
#include <tuple>
#include <typeinfo>

#include "MergeWireSegment.hpp"

namespace {
using namespace std;
using namespace banyan;

void mergeLinesTestBench(
    const string& name,
    function<void(const tuple<const deque<MwsWire>&>&)> cs)
{
  random_device r;
  default_random_engine e1(r());
  uniform_int_distribution<int> uniform_dist(1, 1000000);

  auto rnd = [&](int l, int r) { return uniform_dist(e1) % (r - l + 1) + l; };
  string layerNameList[3] = {"test1", "test2", "test3"};

  auto randomWires = [&](int len, int n, int m) {
    bool isLevel = (rnd(1, 2) == 1);
    deque<MwsWire>wires;
    for (int i = 0; i < len; i++) {
      if (isLevel) {
        int y = rnd(1, m);
        int x1 = rnd(1, n), x2 = rnd(1, n);
        MwsLine line(MwsPoint(x1, y), MwsPoint(x2, y));
        wires.emplace_back(layerNameList[rnd(0, 2)], line);
      } else {
        int x = rnd(1, n);
        int y1 = rnd(1, m), y2 = rnd(1, m);
        MwsLine line(MwsPoint(x, y1), MwsPoint(x, y2));
        wires.emplace_back(layerNameList[rnd(0, 2)], line);
      }

      bool isPoint = (rnd(1,5) == 1);
      if(isPoint){
        int x = rnd(1,n);
        int y = rnd(1,m);
        MwsLine line(MwsPoint(x, y), MwsPoint(x, y));
        wires.emplace_back(layerNameList[rnd(0, 2)], line);
      }
    }
    return wires;
  };

  int smallCases = 100;
  for (int c = 0; c < smallCases; c++) {
    deque<MwsWire> wires = randomWires(49, 7, 7);
    cs({wires});
  }
  int bigCases = 3;
  for(int c = 0;c<bigCases;c++){
    deque<MwsWire> wires = randomWires(625, 25, 25);
    cs({wires});
  }
}

deque<MwsWire> mergeLines(const tuple<const deque<MwsWire>&>& in)
{
  auto wires = get<0>(in);
  return MergeWireSegment::mergeWireLines(wires);
}

void mergeLinesVerifier(const deque<MwsWire>& res,
                        const tuple<const deque<MwsWire>&>& in)
{
  auto getMap = [](const deque<MwsWire>& wires) {
    unordered_map<string, deque<deque<MwsLine>>> mp;
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
  auto getPolygonSet = [](const deque<MwsLine>& lines) {
    polygon_set s;
    for (auto x : lines) {
      auto poly = x.extendRect();
      s.insert(poly);
    }
    return s;
  };

  auto origin = get<0>(in);

  auto oMap = getMap(origin);
  auto rMap = getMap(res);

  // check self
  for (auto& [layer, line] : rMap) {
    for (int s = 0; s <= 1; s++) {
      for (int i = 0; i < line[s].size(); i++) {
        for (int j = i + 1; j < line[s].size(); j++) {
          auto hasOverlapping = MwsLine::IsOverlapping(line[s][i], line[s][j]);
          TESTA_ASSERT(!hasOverlapping)(hasOverlapping).issue();
        }
      }
    }
  }
  // check the same
  for (auto& [layer, line] : oMap) {
    auto rLine = rMap[layer];
    for (int i = 0; i <= 1; i++) {
      auto os = getPolygonSet(line[i]);
      auto rs = getPolygonSet(rLine[i]);
      auto polygonSame
          = boost::polygon::equivalence<polygon_set, polygon_set>(os, rs);
      TESTA_ASSERT(polygonSame)(polygonSame).issue();
    }
  }
}

TESTA_DEF_VERIFY_WITH_TB(MergeWireSegmentTest,
                         mergeLinesTestBench,
                         mergeLinesVerifier,
                         mergeLines);
}  // namespace