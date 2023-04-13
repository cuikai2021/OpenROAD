#include "GridlessMaze.hpp"

#include <ctime>

#include "thread_pool/ThreadPool.h"

namespace banyan {

GridlessMaze::DBox GridlessMaze::getDBoxByBox(const GmBox& b)
{
  return DBox(DPoint(b.xMin, b.yMin), DPoint(b.xMax, b.yMax));
}
GridlessMaze::DBox GridlessMaze::getDBoxByPoint(int x1, int y1, int x2, int y2)
{
  return DBox(DPoint(x1, y1), DPoint(x2, y2));
}

void GridlessMaze::cutTrapezoid(std::vector<GmBox>& boxes)
{
  if (boxes.empty()) {
    return;
  }
  std::vector<GmBox> ans;
  auto layer = boxes[0].layer;
  auto pinName = boxes[0].name;
  polygon_set s;
  for (const auto& b : boxes) {
    s.insert(PinClusterBuilder::GetRectPoly(b.xMin, b.yMin, b.xMax, b.yMax));
  }
  std::vector<polygon> res;
  s.get_trapezoids(res);
  for (const auto& poly : res) {
    int xMin = std::numeric_limits<int>::max();
    int yMin = xMin;
    int xMax = std::numeric_limits<int>::min();
    int yMax = xMax;
    for (const auto& w : poly) {
      xMin = std::min(xMin, w.x());
      xMax = std::max(xMax, w.x());
      yMin = std::min(yMin, w.y());
      yMax = std::max(yMax, w.y());
    }
    ans.emplace_back(xMin, yMin, xMax, yMax, layer, pinName, GmBox::PIN);
  }
  boxes = ans;
}

void GridlessMaze::RemoveIllegal(std::vector<GmVirtualPin>& ans)
{
  std::unordered_set<std::string> pinAnsSet;
  std::vector<GmVirtualPin> checkAns;
  std::sort(ans.begin(),
            ans.end(),
            [](const GmVirtualPin& lhs, const GmVirtualPin& rhs) {
              return lhs.names.size() > rhs.names.size();
            });
  RTree rt;

  for (auto& w : ans) {
    bool ok = true;
    for (auto& name : w.names) {
      if (pinAnsSet.count(name)) {
        ok = false;
        break;
      }
    }
    for (auto b : w.lines) {
      b.extendBox(U_ - 1);
      if (isInTree(rt, b.lx, b.ly, b.rx, b.ry)) {
        ok = false;
        break;
      }
    }
    if (ok) {
      checkAns.push_back(w);
      for (auto& name : w.names) {
        pinAnsSet.insert(name);
      }
      for (auto b : w.lines) {
        b.extendBox(U_ - 1);
        rt.insert(std::make_pair(getDBoxByPoint(b.lx, b.ly, b.rx, b.ry), 0));
      }
    }
  }
  ans.swap(checkAns);
}

std::vector<GmVirtualPin> GridlessMaze::Run()
{
  int enableCPUNum = sysconf(_SC_NPROCESSORS_ONLN);
  ThreadPool pool((enableCPUNum - 1) * 2);

  time_t stTime = time(nullptr);

  std::vector<GmVirtualPin> ans;
  PinClusterBuilder pcb(db_, logger_);
  auto pinClusters = pcb.Build();
  logger_->info(utl::BY, 1, "pinClusters:{}", pinClusters.size());
  if (pinClusters.empty()) {
    return {};
  }

  auto chip = db_->getChip();
  auto block = chip->getBlock();
  auto fLayer = PinClusterBuilder::FindFirstRoutingLayer(db_->getTech());

  manufacturingGrid_ = db_->getTech()->getManufacturingGrid();
  gridU_ = fLayer->getSpacing() + fLayer->getWidth();
  U_ = fLayer->getSpacing() + fLayer->getWidth() / 2;
  lineWidth_ = fLayer->getWidth() / 2;

  std::unordered_map<std::string, std::string> pin2net;
  for (auto net : block->getNets()) {
    if (net->isSpecial()) {
      continue;
    }
    auto netName = net->getName();
    for (auto pin : net->getITerms()) {
      auto pinName = PinClusterBuilder::PinName(pin);
      pin2net[pinName] = netName;
    }
  }

  maxInstWidth_ = std::numeric_limits<int>::min();
  for (auto x : block->getInsts()) {
    maxInstWidth_ = std::max((int) x->getMaster()->getWidth(), maxInstWidth_);
  }

  std::vector<std::future<std::vector<GmVirtualPin>>> futureRes;
  for (const auto& pinCluster : pinClusters) {
    futureRes.emplace_back(pool.enqueue(
        [this](const PinCluster& pc,
               const std::unordered_map<std::string, std::string>& pin2netMap) {
          return this->ConnectPin(pc, pin2netMap);
        },
        std::ref(pinCluster),
        std::ref(pin2net)));
  }

  for (int i = 0; i < futureRes.size(); i++) {
    auto res = futureRes[i].get();
    for (auto& w : res) {
      ans.emplace_back(w);
    }
  }
  RemoveIllegal(ans);
  time_t edTime = time(nullptr);
  logger_->info(utl::BY,
                1,
                "time:{}s   Connect:{}    ",
                (double) (edTime - stTime),
                ans.size());
  return ans;
}

std::vector<GmVirtualPin> GridlessMaze::ConnectPin(
    const PinCluster& pc,
    const std::unordered_map<std::string, std::string>& pin2net)
{
  std::vector<GmVirtualPin> ans;
  std::unordered_map<std::string, std::vector<std::string>> net2pin;
  std::unordered_map<std::string, std::vector<GmBox>> pinBoxes;
  for (const auto& b : pc.boxes()) {
    if (b.type == GmBox::PIN) {
      auto pinName = b.name;
      pinBoxes[pinName].emplace_back(b);
    }
  }
  for (auto& [pinName, boxes] : pinBoxes) {
    const auto& netName = pin2net.at(pinName);
    net2pin[netName].emplace_back(pinName);
  }
  bool needRun = false;
  for (auto& [netName, netPinList] : net2pin) {
    if (netPinList.size() > 1) {
      std::vector<GmBox> tmp;
      needRun = true;
    }
  }
  if (!needRun) {
    return ans;
  }

  auto getX = [this](const GmBox& b) {
    std::vector<int> res{b.xMin, b.xMax};
    if (b.type == GmBox::PIN) {
      res.emplace_back(res[0] + this->lineWidth_);
      res.emplace_back(res[1] - this->lineWidth_);
      res.emplace_back((b.xMin + b.xMax) / 2);
    }
    return res;
  };
  auto getY = [this](const GmBox& b) {
    std::vector<int> res{b.yMin, b.yMax};
    if (b.type == GmBox::PIN) {
      res.emplace_back(res[0] + this->lineWidth_);
      res.emplace_back(res[1] - this->lineWidth_);
      res.emplace_back((b.yMin + b.yMax) / 2);
    }
    return res;
  };
  auto getPinPoint = [&, this](std::vector<PII>& ans,
                               const GmBox& b,
                               int x1,
                               int x2,
                               int y1,
                               int y2) {
    std::vector<PII> tmp;
    tmp.emplace_back(b.xMin, b.yMin + this->lineWidth_);
    tmp.emplace_back(b.xMin, b.yMax - this->lineWidth_);
    tmp.emplace_back(b.xMax, b.yMin + this->lineWidth_);
    tmp.emplace_back(b.xMax, b.yMax - this->lineWidth_);

    tmp.emplace_back(b.xMin + this->lineWidth_, b.yMin);
    tmp.emplace_back(b.xMin + this->lineWidth_, b.yMax);
    tmp.emplace_back(b.xMax - this->lineWidth_, b.yMin);
    tmp.emplace_back(b.xMax - this->lineWidth_, b.yMax);

    tmp.emplace_back(b.xMin, (b.yMin + b.yMax) / 2);
    tmp.emplace_back(b.xMax, (b.yMin + b.yMax) / 2);
    tmp.emplace_back((b.xMin + b.xMax) / 2, b.yMin);
    tmp.emplace_back((b.xMin + b.xMax) / 2, b.yMax);
    for (auto& w : tmp) {
      if (w.first >= x1 && w.first <= x2 && w.second >= y1 && w.second <= y2) {
        ans.emplace_back(w);
      }
    }
  };

  polygon_set obsSet;
  for (auto b : pc.boxes()) {
    if (b.type == GmBox::PIN) {
      continue;
    }
    obsSet.insert(
        PinClusterBuilder::GetRectPoly(b.xMin, b.yMin, b.xMax, b.yMax));
  }
  std::vector<polygon> obsRectList;
  obsSet.get_trapezoids(obsRectList);
  RTree obsTree;
  for (const auto& b : obsRectList) {
    int x1, y1, x2, y2;
    x1 = y1 = std::numeric_limits<int>::max();
    x2 = y2 = std::numeric_limits<int>::min();
    for (const auto& w : b) {
      x1 = std::min(x1, w.x());
      y1 = std::min(y1, w.y());
      x2 = std::max(x2, w.x());
      y2 = std::max(y2, w.y());
    }
    obsTree.insert(std::make_pair(getDBoxByPoint(x1, y1, x2, y2), 0));
  }
  RTree pinTree;
  std::vector<int> totPinExtendPoint[2];
  // 所有pin都是障碍物
  for (auto b : pc.boxes()) {
    if (b.type == GmBox::OBS) {
      b.narrow(U_ - 1);
    }
    totPinExtendPoint[0].emplace_back(b.xMin);
    totPinExtendPoint[0].emplace_back(b.xMax);
    totPinExtendPoint[1].emplace_back(b.yMin);
    totPinExtendPoint[1].emplace_back(b.yMax);
    b.expand(U_ - 1);
    pinTree.insert(std::make_pair(getDBoxByBox(b), 0));
    totPinExtendPoint[0].emplace_back(b.xMin - 1);
    totPinExtendPoint[0].emplace_back(b.xMax + 1);
    totPinExtendPoint[1].emplace_back(b.yMin - 1);
    totPinExtendPoint[1].emplace_back(b.yMax + 1);
  }
  for (int i = 0; i <= 1; i++) {
    std::sort(totPinExtendPoint[i].begin(), totPinExtendPoint[i].end());
    totPinExtendPoint[i].erase(
        std::unique(totPinExtendPoint[i].begin(), totPinExtendPoint[i].end()),
        totPinExtendPoint[i].end());
  }

  std::vector<GmBox> lineBox;

  // type 0:x  1:y
  auto extendHanna = [&, this](std::vector<int>& v, int type, int x1, int x2) {
    std::vector<int> tmp;
    v.insert(
        v.end(),
        std::lower_bound(
            totPinExtendPoint[type].begin(), totPinExtendPoint[type].end(), x1),
        std::upper_bound(totPinExtendPoint[type].begin(),
                         totPinExtendPoint[type].end(),
                         x2));

    for (auto& i : v) {
      if (x1 <= i && i <= x2) {
        tmp.emplace_back(i);
      }
    }
    v.swap(tmp);
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    std::vector<int> ans = v;
    int u = this->gridU_;
    int vLen = (int) v.size();
    for (int i = 1; i < vLen - 1; i++) {
      int lMin = -1;
      for (int k = 0; i - k - 1 >= 0; k++) {
        if (v[i - k] - v[i - k - 1] >= 4 * u) {
          lMin = k;
          break;
        }
      }
      int rMin = vLen;
      for (int k = 0; i + k + 1 < vLen; k++) {
        if (v[i + k + 1] - v[i + k] >= 4 * u) {
          rMin = k;
          break;
        }
      }
      int cMin = i - lMin;
      int cMax = i + rMin;
      if (lMin != -1) {
        for (int j = v[i] - u; j >= v[cMin] - 2 * u; j -= u) {
          if (j < x1) {
            break;
          }
          ans.emplace_back(j);
        }
      }
      if (rMin != vLen) {
        for (int j = v[i] + u; j <= v[cMax] + 2 * u; j += u) {
          if (j > x2) {
            break;
          }
          ans.emplace_back(j);
        }
      }
    }
    std::sort(ans.begin(), ans.end());
    ans.erase(std::unique(ans.begin(), ans.end()), ans.end());
    v = ans;
  };

  for (auto& [netName, netPinList] : net2pin) {
    if (netPinList.size() <= 1) {
      continue;
    }
    // 当前net的pin不是障碍物
    for (const auto& pinName : netPinList) {
      for (auto b : pinBoxes[pinName]) {
        b.expand(U_ - 1);
        pinTree.remove(std::make_pair(getDBoxByBox(b), 0));
      }
    }
    auto pinCmp = [&](const std::string& lhs, const std::string& rhs) {
      int lx = std::numeric_limits<int>::max();
      int ly = lx;
      int rx = lx;
      int ry = lx;
      for (const auto& b : pinBoxes[lhs]) {
        lx = std::min(lx, b.xMin);
        ly = std::min(ly, b.yMin);
      }
      for (const auto& b : pinBoxes[rhs]) {
        rx = std::min(rx, b.xMin);
        ry = std::min(ry, b.yMin);
      }
      if (lx == rx)
        return ly < ry;
      return lx < rx;
    };
    std::sort(netPinList.begin(), netPinList.end(), pinCmp);
    if (netPinList.size() > 1) {
      int lLim = std::numeric_limits<int>::max();
      int rLim = std::numeric_limits<int>::min();

      for (const auto& pinName : netPinList) {
        for (const auto& b : pinBoxes[pinName]) {
          lLim = std::min(lLim, b.xMin);
          rLim = std::max(rLim, b.xMax);
        }
      }

      lLim = std::max(lLim, pc.xMin());
      rLim = std::min(rLim, pc.xMax());
      std::vector<int> xList({lLim, rLim}), yList({pc.yMin(), pc.yMax()});
      std::vector<std::pair<std::string, std::vector<GmBox>>> usePins;
      for (const auto& pinName : netPinList) {
        std::vector<GmBox> tmp;
        for (auto b : pinBoxes[pinName]) {
          tmp.emplace_back(b);
          auto xl = getX(b);
          for (auto& x : xl) {
            xList.emplace_back(x);
          }
          auto yl = getY(b);
          for (auto& y : yl) {
            yList.emplace_back(y);
          }
          b.expand(this->U_ - 1);
          xList.emplace_back(b.xMin - 1);
          xList.emplace_back(b.xMax + 1);
          yList.emplace_back(b.yMin - 1);
          yList.emplace_back(b.yMax + 1);
        }
        usePins.emplace_back(pinName, tmp);
      }

      // add lineBox grid +1-1 走的是膨胀边缘
      for (auto b : lineBox) {
        xList.emplace_back(b.xMin);
        xList.emplace_back(b.xMax);
        yList.emplace_back(b.yMin);
        yList.emplace_back(b.yMax);
        b.expand(U_ - 1);
        xList.emplace_back(b.xMin - 1);
        xList.emplace_back(b.xMax + 1);
        yList.emplace_back(b.yMin - 1);
        yList.emplace_back(b.yMax + 1);
      }
      extendHanna(xList, 0, lLim, rLim);
      extendHanna(yList, 1, pc.yMin(), pc.yMax());

      std::unordered_set<std::string> connectPinNameSet;
      std::vector<GmRoutLine> connectLine;
      for (int k = 1; k < usePins.size(); k++) {
        auto checkSimpleSource
            = [](const std::pair<std::string, std::vector<GmBox>>& lhs,
                 const std::pair<std::string, std::vector<GmBox>>& rhs) {
                if (lhs.second.size() != rhs.second.size()) {
                  return lhs.second.size() < rhs.second.size();
                }
                int h1 = std::numeric_limits<int>::max();
                int h2 = h1;
                for (const auto& b : lhs.second) {
                  h1 = std::min(h1, b.yMax - b.yMin);
                }
                for (const auto& b : rhs.second) {
                  h2 = std::min(h2, b.yMax - b.yMin);
                }
                return h1 < h2;
              };

        bool sp = false;
        if (!checkSimpleSource(usePins[k - 1], usePins[k])) {
          sp = true;
          std::swap(usePins[k - 1], usePins[k]);
        }
        auto sourceName = usePins[k - 1].first;
        auto sinkName = usePins[k].first;
        std::vector<PII> source;
        std::vector<PII> sink;
        std::unordered_set<std::string> curConnectPinNameSet;
        for (const auto& b : usePins[k - 1].second) {
          curConnectPinNameSet.insert(b.name);
          getPinPoint(source, b, lLim, rLim, pc.yMin(), pc.yMax());
        }
        for (const auto& b : usePins[k].second) {
          curConnectPinNameSet.insert(b.name);
          getPinPoint(sink, b, lLim, rLim, pc.yMin(), pc.yMax());
        }

        auto lineRes = findPath(source,
                                sink,
                                usePins[k - 1].second,
                                usePins[k].second,
                                xList,
                                yList,
                                obsTree,
                                pinTree);

        if (!lineRes.empty()) {
          for (auto& w : curConnectPinNameSet) {
            connectPinNameSet.insert(w);
          }
          connectLine.insert(connectLine.end(), lineRes.begin(), lineRes.end());
        }
        if (lineRes.empty() || k == (int) usePins.size() - 1) {
          std::vector<std::string> names;
          for (auto& x : connectPinNameSet) {
            names.emplace_back(x);
          }
          connectPinNameSet.clear();
          if (!connectLine.empty()) {
            ans.push_back({netName, names, connectLine});
          }
          connectLine.clear();
        }
        for (auto& x : lineRes) {
          auto nb = x.extendBox(lineWidth_);
          nb.expand(U_ - 1);
          obsTree.insert(std::make_pair(getDBoxByBox(nb), 0));
          nb.narrow(U_ - 1);
          lineBox.emplace_back(nb);
        }
        // 还原
        if (sp) {
          std::swap(usePins[k - 1], usePins[k]);
        }
      }
    }
    // 当前net的pin是障碍物
    for (const auto& pinName : netPinList) {
      for (auto b : pinBoxes[pinName]) {
        b.expand(U_ - 1);
        pinTree.insert(std::make_pair(getDBoxByBox(b), 0));
      }
    }
  }

  return ans;
}

std::vector<GmRoutLine> GridlessMaze::findPath(const std::vector<PII>& source,
                                               const std::vector<PII>& sink,
                                               std::vector<GmBox> sourceBox,
                                               std::vector<GmBox> sinkBox,
                                               const std::vector<int>& xList,
                                               const std::vector<int>& yList,
                                               const RTree& obsTree,
                                               const RTree& pinTree)
{
  cutTrapezoid(sourceBox);
  cutTrapezoid(sinkBox);
  const int WEST = 0, SOUTH = 1, EAST = 2, NORTH = 3;
  const int TOTDIR = 15;
  int nextX[4] = {-1, 0, 1, 0};
  int nextY[4] = {0, -1, 0, 1};
  int invDir[4] = {2, 3, 0, 1};

  std::unordered_map<GmMazeNodeHashVal, bool, GmMazeNodeHash> vis;
  std::unordered_map<PII, GmMazeNode, PIIHash> pre[16];
  std::unordered_map<PII, bool, PIIHash> obsPoint;
  std::unordered_map<int, int> xMap, yMap;

  for (int i = 0; i < xList.size(); i++) {
    xMap[xList[i]] = i;
  }
  for (int i = 0; i < yList.size(); i++) {
    yMap[yList[i]] = i;
  }

  for (auto& [x, y] : sink) {
    assert(xMap.count(x) && yMap.count(y));
  }

  auto calcDis = [&](int x1, int y1, int x2, int y2) {
    return abs(xList[x1] - xList[x2]) + abs(yList[y1] - yList[y2]);
  };

  auto calcDisToSink = [&](int x1, int y1) {
    x1 = xList[x1];
    y1 = yList[y1];
    int a1 = std::numeric_limits<int>::max();
    int a2 = a1;
    for (const auto& b : sinkBox) {
      int s1 = std::min(abs(b.xMin - x1), abs(b.xMax - x1));
      if (b.xMin <= x1 && x1 <= b.xMax) {
        s1 = 0;
      }
      int s2 = std::min(abs(b.yMin - y1), abs(b.yMax - y1));
      if (b.yMin <= y1 && y1 <= b.yMax) {
        s2 = 0;
      }
      a1 = std::min(a1, s1 + s2);
      a2 = std::min({a2, abs(x1 - b.xMin), abs(x1 - b.xMax)});
    }
    return std::make_tuple(a1, a2);
  };

  auto delDir = [](int& x, const std::vector<int>& dir) {
    for (auto w : dir) {
      x ^= (1 << w);
    }
  };

  auto isTurn = [&](int x, int y) {
    if (x == WEST || x == EAST) {
      return (y == SOUTH || y == NORTH);
    }
    if (x == SOUTH || x == NORTH) {
      return (y == WEST || y == EAST);
    }
    return false;
  };

  auto canGo = [](int x, int dir) { return ((x >> dir) & 1); };
  auto checkDrcInPin
      = [&, this](int x, int y, int dir, const std::vector<GmBox>& boxes) {
          x = xList[x];
          y = yList[y];
          int w = this->lineWidth_;
          if (dir == -1) {
            return false;
          }
          bool ok = false;
          for (const auto& b : boxes) {
            int x1 = b.xMin;
            int x2 = b.xMax;
            int y1 = b.yMin;
            int y2 = b.yMax;
            if (x1 <= x && x <= x2 && y1 <= y && y <= y2) {
              if (dir == WEST || dir == EAST) {
                y1 += w;
                y2 -= w;
                if (dir == WEST) {
                  x1 += w;
                } else {
                  x2 -= w;
                }
              } else {
                x1 += w;
                x2 -= w;
                if (dir == NORTH) {
                  y2 -= w;
                } else {
                  y1 += w;
                }
              }
              if (x1 <= x && x <= x2 && y1 <= y && y <= y2) {
                ok = true;
              } else {
                return false;
              }
            }
          }
          return ok;
        };

  auto checkLineDrc = [&, this](const GmRoutLine& line) {
    int x1 = xList[line.lx];
    int x2 = xList[line.rx];
    int y1 = yList[line.ly];
    int y2 = yList[line.ry];
    int U = this->U_;
    int lw = this->lineWidth_;

    auto lineIntersectLenght = [&](int l1, int r1, int l2, int r2) {
      int l = std::max(l1, l2);
      int r = std::min(r1, r2);
      return std::max(0, r - l);
    };

    auto checkLevel = [&](const GmBox& b) {
      if (b.contains(x1, y1) || b.contains(x2, y2)) {
        return true;
      }
      if (lineIntersectLenght(x1 - lw / 2, x2 + lw / 2, b.xMin, b.xMax) > 0) {
        if ((abs(b.yMin - y1) < U && abs(b.yMin - y1) > lw)
            || (abs(b.yMax - y1) < U && abs(b.yMax - y1) > lw)) {
          return false;
        }
      }
      return true;
    };

    auto checkVertical = [&, this](const GmBox& b) {
      if (b.contains(x1, y1) || b.contains(x2, y2)) {
        return true;
      }
      if (lineIntersectLenght(y1 - lw / 2, y2 + lw / 2, b.yMin, b.yMax)) {
        if ((abs(b.xMin - x1) < U && abs(b.xMin - x1) > lw)
            || (abs(b.xMax - x1) < U && abs(b.xMax - x1) > lw)) {
          return false;
        }
      }
      return true;
    };

    bool isLevel = line.isLevel();
    for (const auto& b : sourceBox) {
      bool ok = true;
      if (isLevel) {
        ok = checkLevel(b);
      } else {
        ok = checkVertical(b);
      }
      if (!ok) {
        return false;
      }
    }
    for (const auto& b : sinkBox) {
      bool ok = true;
      if (isLevel) {
        ok = checkLevel(b);
      } else {
        ok = checkVertical(b);
      }
      if (!ok) {
        return false;
      }
    }
    return true;
  };

  auto isInPin = [&, this](int x, int y, const std::vector<GmBox>& boxes) {
    // 越界
    if (x < 0 || x >= xList.size() || y < 0 || y >= yList.size()) {
      return false;
    }
    x = xList[x];
    y = yList[y];
    for (const auto& b : boxes) {
      if (b.contains(x, y)) {
        return true;
      }
    }
    return false;
  };
  auto isInPinBoundary = [&](int x, int y, const std::vector<GmBox>& boxes) {
    x = xList[x];
    y = yList[y];
    for (const auto& b : boxes) {
      if (x == b.xMin || x == b.xMax) {
        if (b.yMin <= y && y <= b.yMax) {
          return true;
        }
      }
      if (y == b.yMin || y == b.yMax) {
        if (b.xMin <= x && x <= b.xMax) {
          return true;
        }
      }
    }
    return false;
  };

  auto checkOffGrid = [&](int x, int y) {
    if (xList[x] % manufacturingGrid_ || yList[y] % manufacturingGrid_) {
      return false;
    }
    return true;
  };

  auto check = [&, this](const GmMazeNode& cur, GmMazeNode& nex) {
    // 越界
    if (nex.x < 0 || nex.x >= xList.size() || nex.y < 0
        || nex.y >= yList.size()) {
      return false;
    }

    // OBS and other pin
    if (!obsPoint.count(std::make_pair(xList[nex.x], yList[nex.y]))) {
      auto inObs = isInTree(obsTree, xList[nex.x], yList[nex.y]);
      auto inOtherPin = isInTree(pinTree, xList[nex.x], yList[nex.y]);
      if (inOtherPin || inObs) {
        obsPoint[std::make_pair(xList[nex.x], yList[nex.y])] = true;
      } else {
        obsPoint[std::make_pair(xList[nex.x], yList[nex.y])] = false;
      }
    }
    if (obsPoint[std::make_pair(xList[nex.x], yList[nex.y])]) {
      return false;
    }

    // 二次进入起点
    if (!isInPin(cur.x, cur.y, sourceBox) && isInPin(nex.x, nex.y, sourceBox)) {
      return false;
    }

    // 起点出去的时候
    if (isInPinBoundary(cur.x, cur.y, sourceBox)
        && !isInPinBoundary(nex.x, nex.y, sourceBox)) {
      if (!checkDrcInPin(cur.x, cur.y, invDir[nex.preDir], sourceBox)
          || !checkOffGrid(cur.x, cur.y)) {
        return false;
      }
    }

    for (const auto& b : sinkBox) {
      if (b.contains(xList[nex.x], yList[nex.y])) {
        switch (nex.preDir) {
          case WEST:
            nex.x = xMap[b.xMax];
            break;
          case SOUTH:
            nex.y = yMap[b.yMax];
            break;
          case EAST:
            nex.x = xMap[b.xMin];
            break;
          case NORTH:
            nex.y = yMap[b.yMin];
          default:
            break;
        }
        if (!checkOffGrid(nex.x, nex.y)) {
          return false;
        }
      }
    }

    if (!checkLineDrc({"",
                       std::min(cur.x, nex.x),
                       std::min(cur.y, nex.y),
                       std::max(cur.x, nex.x),
                       std::max(cur.y, nex.y)})) {
      return false;
    }

    // 是否访问过
    if (vis.count(std::make_tuple(nex.x, nex.y, nex.sta))) {
      return false;
    }

    return true;
  };

  auto initBig = [](std::vector<int>& v, const std::vector<int>& a, int x) {
    v.resize(a.size());
    for (int& i : v) {
      i = -1;
    }
    int j = 0;
    for (int i = 0; i < a.size(); i++) {
      while (j < a.size() && a[j] - a[i] < x) {
        ++j;
      }
      if (j < a.size() && a[j] - a[i] >= x) {
        v[i] = j;
      }
    }
  };

  auto initBigFar = [](std::vector<int>& v, const std::vector<int>& a, int x) {
    v.resize(a.size());
    for (int& i : v) {
      i = -1;
    }
    int j = 0;
    for (int i = 0; i < a.size(); i++) {
      while (j < a.size() && a[j] - a[i] <= x) {
        ++j;
      }
      if (a[j - 1] - a[i] <= x) {
        v[i] = j - 1;
      }
    }
  };

  auto initSmall = [](std::vector<int>& v, const std::vector<int>& a, int x) {
    v.resize(a.size());
    for (int& i : v) {
      i = -1;
    }
    int j = (int) a.size() - 1;
    for (int i = (int) a.size() - 1; i >= 0; i--) {
      while (j >= 0 && a[i] - a[j] < x) {
        --j;
      }
      if (j >= 0 && a[i] - a[j] >= x) {
        v[i] = j;
      }
    }
  };

  auto initSmallFar
      = [](std::vector<int>& v, const std::vector<int>& a, int x) {
          v.resize(a.size());
          for (int& i : v) {
            i = -1;
          }
          int j = (int) a.size() - 1;
          for (int i = (int) a.size() - 1; i >= 0; i--) {
            while (j >= 0 && a[i] - a[j] <= x) {
              --j;
            }
            if (a[i] - a[j + 1] <= x) {
              v[i] = j + 1;
            }
          }
        };

  std::vector<int> jumpGirdU[4];
  initBig(jumpGirdU[EAST], xList, gridU_);
  initBig(jumpGirdU[NORTH], yList, gridU_);
  initSmall(jumpGirdU[WEST], xList, gridU_);
  initSmall(jumpGirdU[SOUTH], yList, gridU_);
  std::vector<int> jumpFar[4];
  initBigFar(jumpFar[EAST], xList, 2 * U_ - 1);
  initBigFar(jumpFar[NORTH], yList, 2 * U_ - 1);
  initSmallFar(jumpFar[WEST], xList, 2 * U_ - 1);
  initSmallFar(jumpFar[SOUTH], yList, 2 * U_ - 1);

  std::priority_queue<GmMazeNode> q;
  for (const auto& [x, y] : source) {
    if (isInTree(obsTree, x, y) || isInTree(pinTree, x, y)
        || !isInPinBoundary(xMap[x], yMap[y], sourceBox)
        || !checkOffGrid(xMap[x], yMap[y])) {
      continue;
    }
    q.push({xMap[x], yMap[y], 0, calcDisToSink(xMap[x], yMap[y]), TOTDIR, -1});
    vis[std::make_tuple(xMap[x], yMap[y], TOTDIR)] = true;
  }

  GmMazeNode end{};
  end.sta = -1;
  int count = 0;
  while (!q.empty() && end.sta == -1) {
    if (count > searchLimit_) {
      break;
    }
    ++count;
    auto cur = q.top();
    q.pop();

    if (checkDrcInPin(cur.x, cur.y, cur.preDir, sinkBox)) {
      end = cur;
      break;
    }

    int preDir = cur.preDir;
    for (int i = 0; i < 4; i++) {
      bool turn = isTurn(preDir, i);
      if (canGo(cur.sta, i)) {
        auto k = cur;

        auto solveNewNode = [&]() {
          k.sta = 15;
          if (i != k.preDir) {
            k.preDir = i;
          }
          delDir(k.sta, {invDir[i]});
          if (check(cur, k)) {
            int dch = calcDis(k.x, k.y, cur.x, cur.y);
            k.d += dch;
            if (isInPinBoundary(k.x, k.y, sourceBox)) {
              k.d = 0;
            }
            k.w = calcDisToSink(k.x, k.y);
            std::get<0>(k.w) += k.d;
            vis[std::make_tuple(k.x, k.y, k.sta)] = true;
            pre[k.sta][std::make_pair(k.x, k.y)] = cur;
            q.push(k);
          }
        };
        if (!turn) {
          k.x += nextX[i];
          k.y += nextY[i];
          solveNewNode();
          k = cur;
          int p = -1;
          if (i == EAST || i == WEST) {
            p = jumpFar[i][k.x];
            k.x = p;
          } else {
            p = jumpFar[i][k.y];
            k.y = p;
          }
          if (p != -1) {
            solveNewNode();
          }
        }

        if (xList[cur.x] % manufacturingGrid_
            || yList[cur.y] % manufacturingGrid_) {
          continue;
        }
        k = cur;
        int p = -1;
        if (i == EAST || i == WEST) {
          p = jumpGirdU[i][k.x];
          k.x = p;
        } else {
          p = jumpGirdU[i][k.y];
          k.y = p;
        }
        if (p != -1) {
          solveNewNode();
        }
      }
    }
  }
  std::vector<GmRoutLine> res;
  if (end.sta == -1) {
    return res;
  }
  std::vector<std::pair<int, int>> pList;
  pList.emplace_back(end.x, end.y);
  while (pre[end.sta].count(std::make_pair(end.x, end.y))) {
    auto p = pre[end.sta][std::make_pair(end.x, end.y)];
    pList.emplace_back(p.x, p.y);
    if (isInPinBoundary(p.x, p.y, sourceBox)) {
      break;
    }
    end = p;
  }
  std::reverse(pList.begin(), pList.end());
  for (int i = 0; i < (int) pList.size() - 1; i++) {
    int j = i + 1;
    if (pList[j].first == pList[i].first) {
      while (j < pList.size() && pList[j].first == pList[i].first) {
        ++j;
      }
    } else {
      while (j < pList.size() && pList[j].second == pList[i].second) {
        ++j;
      }
    }
    res.push_back({sinkBox[0].layer->getName(),
                   std::min(pList[i].first, pList[j - 1].first),
                   std::min(pList[i].second, pList[j - 1].second),
                   std::max(pList[i].first, pList[j - 1].first),
                   std::max(pList[i].second, pList[j - 1].second)});
    i = j - 2;
  }
  for (auto& x : res) {
    assert(checkOffGrid(x.lx, x.ly));
    assert(checkOffGrid(x.rx, x.ry));
    x.lx = xList[x.lx];
    x.ly = yList[x.ly];
    x.rx = xList[x.rx];
    x.ry = yList[x.ry];
  }

  return res;
}

bool GridlessMaze::isInTree(const RTree& rt, int x1, int y1, int x2, int y2)
{
  DBox query_box(getDBoxByPoint(x1, y1, x2, y2));
  std::vector<Value> res;
  rt.query(boost::geometry::index::intersects(query_box),
           std::back_inserter(res));
  return !res.empty();
}

bool GridlessMaze::isInTree(const GridlessMaze::RTree& rt, int x, int y)
{
  DBox query_box(DPoint(x, y), DPoint(x, y));
  std::vector<Value> res;
  rt.query(boost::geometry::index::intersects(query_box),
           std::back_inserter(res));
  return !res.empty();
}

void GridlessMaze::updateDB(odb::dbDatabase* db,
                            const std::vector<GmVirtualPin>& vpins)
{
  MergeWireSegment::netWireMap res;
  for (const auto& vp : vpins) {
    for (const auto& line : vp.lines) {
      MwsWire w(
          line.layerName,
          MwsLine(MwsPoint(line.lx, line.ly), MwsPoint(line.rx, line.ry)));
      res[vp.netName].emplace_back(w);
    }
  }
  MergeWireSegment::updateDB(db, res);
}

}  // namespace banyan