#include "GridlessMaze.hpp"
#include "fassert.hpp"
#include <ctime>

#include "thread_pool/ThreadPool.h"

namespace banyan {

InstPinName getInstPinName(const ByBox& b)
{
  return {b.getInstName(), b.getPinName()};
}

GridlessMaze::GridlessMaze(odb::dbDatabase* db,
                           const std::shared_ptr<ByDb>& bydb,
                           utl::Logger* logger,
                           SEARCH_LIMIT searchLimit)
    : db_(db), bydb_(bydb), logger_(logger), searchLimit_(searchLimit)
{
  auto fLayer = ByAdapter::findFirstRoutingLayer(db_->getTech());
  spacing_ = fLayer->getSpacing();
  spacing_ = std::max(spacing_, ByAdapter::getEndOfLineSpacing(fLayer));
  lineWidth_ = fLayer->getWidth();
  manufacturingGrid_ = db_->getTech()->getManufacturingGrid();
  U_ = spacing_ + lineWidth_ / 2;
  GridU_ = spacing_ + lineWidth_;
  logger->info(utl::BY,
               1,
               "spacing:{} lineWidth:{} manufacturingGrid:{} U:{} gu:{}",
               spacing_,
               lineWidth_,
               manufacturingGrid_,
               U_,
               GridU_);
}

std::vector<GmVirtualPin> GridlessMaze::run()
{
  auto rowPcs = PinClusterBuilder(db_, bydb_, logger_).getPinClusters();
  auto flayer = ByAdapter::findFirstRoutingLayer(db_->getTech())->getName();
  std::unordered_map<InstPinName, std::string, pair_hash> pin2Net;
  for (const auto& net : bydb_->getNets()) {
    if (net->isSpecial()) {
      continue;
    }
    for (const auto& pin : net->getPins()) {
      pin2Net[{pin->getInstName(), pin->getPinName()}] = net->getName();
    }
  }

  time_t st = time(nullptr);
  int enableCPUNum = sysconf(_SC_NPROCESSORS_ONLN);
  ThreadPool pool((enableCPUNum - 1) * 2);
  std::vector<std::future<std::vector<GmVirtualPin>>> fres;
  std::vector<GmVirtualPin> ans;
  for (int i = 0; i < rowPcs.size(); i++) {
    fres.emplace_back(pool.enqueue(
        [this](
            std::vector<PinCluster>& rowPc,
            const std::unordered_map<InstPinName, std::string, pair_hash>& mp) {
          return this->solveOneRowPinCluster(rowPc, mp);
        },
        std::ref(rowPcs[i]),
        std::ref(pin2Net)));
  }
  for (auto& x : fres) {
    auto res = x.get();
    ans.insert(ans.end(), res.begin(), res.end());
  }

  std::vector<GmVirtualPin> tmp;
  for (auto& x : ans) {
    bool ok = true;
    for (int i = 0; i < x.lines.size() && ok; i++) {
      auto a = x.lines[i].toBox(lineWidth_);
      for (int j = i + 2; j < x.lines.size() && ok; j++) {
        auto b = x.lines[j].toBox(lineWidth_);
        if (a.intersects(b)) {
          ok = false;
        }
      }
    }
    if (!ok) {
      continue;
    }
    tmp.emplace_back(x);
    for (auto& line : x.lines) {
      line.layerName = flayer;
    }
  }
  ans.swap(tmp);

  double wireLength = 0;
  int pinCnt = 0;
  for (auto& x : ans) {
    tmp.emplace_back(x);
    pinCnt += x.names.size();
    for (auto& line : x.lines) {
      line.layerName = flayer;
      wireLength += abs(line.x1 - line.x2) + abs(line.y1 - line.y2);
    }
  }

  logger_->info(utl::BY,
                1,
                "connect:{} pin:{} wireLength:{} time:{}",
                ans.size(),
                pinCnt,
                wireLength / db_->getChip()->getBlock()->getDbUnitsPerMicron(),
                time(nullptr) - st);

  return ans;
}

std::vector<GmVirtualPin> GridlessMaze::solveOneRowPinCluster(
    std::vector<PinCluster>& pcs,
    const std::unordered_map<InstPinName, std::string, pair_hash>& pin2Net)
{
  std::vector<GmVirtualPin> ans;
  using Value = std::pair<ByBox, int>;
  std::vector<Value> vals;
  for (int i = 0; i < pcs.size(); i++) {
    vals.emplace_back(pcs[i].bbox(), i);
  }
  bgi::rtree<Value, bgi::rstar<32>> rt(vals.begin(), vals.end());
  for (int i = 0; i < pcs.size(); i++) {
    auto res = solvePinCluster(pcs[i], pin2Net);
    for (const auto& vp : res) {
      for (const auto& line : vp.lines) {
        std::vector<Value> tmp;
        auto lb = line.toBox(lineWidth_);
        lb.expand(U_ - 1);
        rt.query(bgi::intersects(lb), std::back_inserter(tmp));
        for (auto& [box, id] : tmp) {
          if (id > i) {
            pcs[id].boxes().emplace_back(lb);
          }
        }
      }
    }
    ans.insert(ans.end(), res.begin(), res.end());
  }
  return ans;
}

std::vector<GmVirtualPin> GridlessMaze::solvePinCluster(
    PinCluster& pc,
    const std::unordered_map<InstPinName, std::string, pair_hash>& pin2Net)
{
  std::vector<GmVirtualPin> ans;
  markTouchWirePin(pc);
  std::unordered_map<std::string, std::vector<InstPinName>> net2Pins;
  std::unordered_map<InstPinName, std::vector<ByBox>, pair_hash> pinBoxes;

  for (const auto& b : pc.boxes()) {
    if (b.getType() == ByBoxInfo::INST_PIN) {
      pinBoxes[getInstPinName(b)].emplace_back(b);
    }
  }

  for (const auto& [pinName, boxes] : pinBoxes) {
    if (pin2Net.count(pinName)) {
      auto net = pin2Net.at(pinName);
      net2Pins[net].emplace_back(pinName);
    }
  }

  BsPolygon90Set obsSet;
  for (auto& b : pc.boxes()) {
    if (b.getType() != ByBoxInfo::INST_PIN) {
      b.expand(U_ - 1);
      obsSet.insert(b);
    }
  }

  std::vector<ByBox> obsBox;
  obsSet.get_rectangles(obsBox);
  Rtree obsRtree(obsBox.begin(), obsBox.end());

  std::vector<int> totPinExtendPoint[2];
  Rtree pinRtree;
  for (auto b : pc.boxes()) {
    if (b.getType() != ByBoxInfo::INST_PIN) {
      b.narrow(U_ - 1);
    }
    totPinExtendPoint[0].emplace_back(b.xMin());
    totPinExtendPoint[0].emplace_back(b.xMax());
    totPinExtendPoint[1].emplace_back(b.yMin());
    totPinExtendPoint[1].emplace_back(b.yMax());
    b.expand(U_ - 1);
    pinRtree.insert(b);
    totPinExtendPoint[0].emplace_back(b.xMin() - 1);
    totPinExtendPoint[0].emplace_back(b.xMax() + 1);
    totPinExtendPoint[1].emplace_back(b.yMin() - 1);
    totPinExtendPoint[1].emplace_back(b.yMax() + 1);
  }

  for (int i = 0; i <= 1; i++) {
    std::sort(totPinExtendPoint[i].begin(), totPinExtendPoint[i].end());
    totPinExtendPoint[i].erase(
        std::unique(totPinExtendPoint[i].begin(), totPinExtendPoint[i].end()),
        totPinExtendPoint[i].end());
  }

  std::vector<ByBox> lineBox;

  auto removePin = [&](const std::vector<ByBox>& boxes) {
    for (auto b : boxes) {
      b.expand(U_ - 1);
      pinRtree.remove(b);
    }
  };
  auto insertPin = [&](const std::vector<ByBox>& boxes) {
    for (auto b : boxes) {
      b.expand(U_ - 1);
      pinRtree.insert(b);
    }
  };

  for (auto& [netName, netPinList] : net2Pins) {
    if (netPinList.size() < 2) {
      continue;
    }
    std::sort(netPinList.begin(),
              netPinList.end(),
              [&](const InstPinName& lhs, const InstPinName& rhs) {
                int lx = std::numeric_limits<int>::max();
                int ly = lx;
                int rx = lx;
                int ry = lx;
                for (const auto& b : pinBoxes[lhs]) {
                  lx = std::min(lx, b.xMin());
                  ly = std::min(ly, b.yMin());
                }
                for (const auto& b : pinBoxes[rhs]) {
                  rx = std::min(rx, b.xMin());
                  ry = std::min(ry, b.yMin());
                }
                if (lx == rx)
                  return ly < ry;
                return lx < rx;
              });

    int lLim = std::numeric_limits<int>::max();
    int rLim = std::numeric_limits<int>::min();

    for (const auto& pinName : netPinList) {
      for (const auto& b : pinBoxes[pinName]) {
        lLim = std::min(lLim, b.xMin());
        rLim = std::max(rLim, b.xMax());
      }
    }

    lLim = std::max(lLim, pc.bbox().xMin());
    rLim = std::min(rLim, pc.bbox().xMax());
    std::vector<int> xList{lLim, rLim},
        yList{pc.bbox().yMin(), pc.bbox().yMax()};
    std::vector<std::pair<InstPinName, std::vector<ByBox>>> usePins;
    for (const auto& pinName : netPinList) {
      std::vector<ByBox> tmp;
      for (auto b : pinBoxes[pinName]) {
        tmp.emplace_back(b);
        auto xl = getXVal(b);
        for (auto& x : xl) {
          xList.emplace_back(x);
        }
        auto yl = getYVal(b);
        for (auto& y : yl) {
          yList.emplace_back(y);
        }
      }
      usePins.emplace_back(pinName, tmp);
    }
    for (auto b : lineBox) {
      xList.emplace_back(b.xMin());
      xList.emplace_back(b.xMax());
      yList.emplace_back(b.yMin());
      yList.emplace_back(b.yMax());
      b.expand(U_ - 1);
      xList.emplace_back(b.xMin() - 1);
      xList.emplace_back(b.xMax() + 1);
      yList.emplace_back(b.yMin() - 1);
      yList.emplace_back(b.yMax() + 1);
    }

    extendHanan(xList, totPinExtendPoint[0], lLim, rLim);
    extendHanan(
        yList, totPinExtendPoint[1], pc.bbox().yMin(), pc.bbox().yMax());

    std::unordered_set<InstPinName, pair_hash> connectPinNameSet;
    std::vector<GmRoutingLine> connectLine;
    for (int k = 1; k < usePins.size(); k++) {
      bool sp = false;
      if (!checkSimpleSource(usePins[k - 1], usePins[k])) {
        sp = true;
        std::swap(usePins[k - 1], usePins[k]);
      }
      std::vector<PII> source;
      std::vector<PII> sink;
      std::unordered_set<std::string> curConnectPinNameSet;
      for (const auto& b : usePins[k - 1].second) {
        getPinPoint(source, b, lLim, rLim, pc.bbox().yMin(), pc.bbox().yMax());
      }
      for (const auto& b : usePins[k].second) {
        getPinPoint(sink, b, lLim, rLim, pc.bbox().yMin(), pc.bbox().yMax());
      }
      removePin(usePins[k - 1].second);
      removePin(usePins[k].second);
      auto lineRes = findPath(netName,
                              source,
                              sink,
                              usePins[k - 1].second,
                              usePins[k].second,
                              xList,
                              yList,
                              obsRtree,
                              pinRtree);

      insertPin(usePins[k - 1].second);
      insertPin(usePins[k].second);

      if (!lineRes.empty()) {
        connectPinNameSet.insert(usePins[k - 1].first);
        connectPinNameSet.insert(usePins[k].first);
        connectLine.insert(connectLine.end(), lineRes.begin(), lineRes.end());
      }

      if (lineRes.empty() || k == (int) usePins.size() - 1) {
        std::vector<InstPinName> names;
        for (auto& x : connectPinNameSet) {
          names.emplace_back(x);
        }
        connectPinNameSet.clear();
        if (!connectLine.empty()) {
          ans.push_back({netName, names, connectLine});
        }
        connectLine.clear();
      }

      for (const auto& w : lineRes) {
        ByBox nb = w.toBox(lineWidth_);
        nb.expand(U_ - 1);
        obsRtree.insert(nb);
        nb.narrow(U_ - 1);
        lineBox.emplace_back(nb);
      }
      // 还原
      if (sp) {
        std::swap(usePins[k - 1], usePins[k]);
      }
    }
  }

  return ans;
}
void GridlessMaze::markTouchWirePin(PinCluster& pc)
{
  std::vector<ByBox> wireBoxes;
  for (const auto& b : pc.boxes()) {
    if (b.getType() == ByBoxInfo::WIRE) {
      wireBoxes.emplace_back(b);
    }
  }
  bgi::rtree<ByBox, bgi::rstar<4>> rt(wireBoxes.begin(), wireBoxes.end());
  auto isTouchWire = [&](const auto& b) {
    std::vector<ByBox> res;
    rt.query(boost::geometry::index::intersects(b), std::back_inserter(res));
    return !res.empty();
  };

  std::set<InstPinName> tmp;
  for (const auto& b : pc.boxes()) {
    if (b.getType() == ByBoxInfo::INST_PIN && isTouchWire(b)) {
      tmp.insert({b.getInstName(), b.getPinName()});
    }
  }
  for (auto& b : pc.boxes()) {
    if (b.getType() == ByBoxInfo::INST_PIN) {
      auto w = std::make_pair(b.getInstName(), b.getPinName());
      if (tmp.count(w)) {
        b = ByBox(b.xMin(),
                  b.yMin(),
                  b.xMax(),
                  b.yMax(),
                  std::make_shared<ByBoxInfo>(b.getLayerName(),
                                              std::nullopt,
                                              std::nullopt,
                                              std::nullopt,
                                              std::nullopt,
                                              ByBoxInfo::INST_OBS));
      }
    }
  }
}
std::vector<int> GridlessMaze::getXVal(const ByBox& b)
{
  std::vector<int> res{b.xMin(), b.xMax()};
  if (b.getType() == ByBoxInfo::INST_PIN) {
    res.emplace_back(res[0] + this->lineWidth_ / 2);
    res.emplace_back(res[1] - this->lineWidth_ / 2);
    res.emplace_back((b.xMin() + b.xMax()) / 2);
  }
  return res;
}
std::vector<int> GridlessMaze::getYVal(const ByBox& b)
{
  std::vector<int> res{b.yMin(), b.yMax()};
  if (b.getType() == ByBoxInfo::INST_PIN) {
    res.emplace_back(res[0] + this->lineWidth_ / 2);
    res.emplace_back(res[1] - this->lineWidth_ / 2);
    res.emplace_back((b.yMin() + b.yMax()) / 2);
  }
  return res;
}
void GridlessMaze::getPinPoint(std::vector<PII>& ans,
                               const ByBox& b,
                               int x1,
                               int x2,
                               int y1,
                               int y2)
{
  std::vector<PII> tmp;
  tmp.emplace_back(b.xMin(), b.yMin() + this->lineWidth_ / 2);
  tmp.emplace_back(b.xMin(), b.yMax() - this->lineWidth_ / 2);
  tmp.emplace_back(b.xMax(), b.yMin() + this->lineWidth_ / 2);
  tmp.emplace_back(b.xMax(), b.yMax() - this->lineWidth_ / 2);

  tmp.emplace_back(b.xMin() + this->lineWidth_ / 2, b.yMin());
  tmp.emplace_back(b.xMin() + this->lineWidth_ / 2, b.yMax());
  tmp.emplace_back(b.xMax() - this->lineWidth_ / 2, b.yMin());
  tmp.emplace_back(b.xMax() - this->lineWidth_ / 2, b.yMax());

  tmp.emplace_back(b.xMin(), (b.yMin() + b.yMax()) / 2);
  tmp.emplace_back(b.xMax(), (b.yMin() + b.yMax()) / 2);
  tmp.emplace_back((b.xMin() + b.xMax()) / 2, b.yMin());
  tmp.emplace_back((b.xMin() + b.xMax()) / 2, b.yMax());
  for (auto& w : tmp) {
    if (w.first >= x1 && w.first <= x2 && w.second >= y1 && w.second <= y2) {
      ans.emplace_back(w);
    }
  }
}
void GridlessMaze::extendHanan(std::vector<int>& v,
                               const std::vector<int>& totPinVal,
                               int x1,
                               int x2)
{
  std::vector<int> tmp;
  v.insert(v.end(),
           std::lower_bound(totPinVal.begin(), totPinVal.end(), x1),
           std::upper_bound(totPinVal.begin(), totPinVal.end(), x2));

  for (auto& i : v) {
    if (x1 <= i && i <= x2) {
      tmp.emplace_back(i);
    }
  }
  v.swap(tmp);
  std::sort(v.begin(), v.end());
  v.erase(std::unique(v.begin(), v.end()), v.end());
  std::vector<int> ans = v;
  int u = GridU_;
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
}
bool GridlessMaze::checkSimpleSource(
    const std::pair<InstPinName, std::vector<ByBox>>& lhs,
    const std::pair<InstPinName, std::vector<ByBox>>& rhs)
{
  if (lhs.second.size() != rhs.second.size()) {
    return lhs.second.size() < rhs.second.size();
  }
  int h1 = std::numeric_limits<int>::max();
  int h2 = h1;
  for (const auto& b : lhs.second) {
    h1 = std::min(h1, b.yMax() - b.yMin());
  }
  for (const auto& b : rhs.second) {
    h2 = std::min(h2, b.yMax() - b.yMin());
  }
  return h1 < h2;
}
std::vector<GmRoutingLine> GridlessMaze::findPath(
    const std::string& netName,
    const std::vector<PII>& source,
    const std::vector<PII>& sink,
    std::vector<ByBox> sourceBox,
    std::vector<ByBox> sinkBox,
    const std::vector<int>& xList,
    const std::vector<int>& yList,
    const GridlessMaze::Rtree& obsTree,
    const GridlessMaze::Rtree& pinTree)
{
  sourceBox = getMaxRect(sourceBox);
  sinkBox = getMaxRect(sinkBox);
  auto sourceV = getRect(sourceBox, bsp::VERTICAL);
  auto sourceH = getRect(sourceBox, bsp::HORIZONTAL);
  auto sinkV = getRect(sinkBox, bsp::VERTICAL);
  auto sinkH = getRect(sinkBox, bsp::HORIZONTAL);
  static const int TOTDIR = 15;
  static int nextX[4] = {-1, 0, 1, 0};
  static int nextY[4] = {0, -1, 0, 1};
  static int invDir[4] = {2, 3, 0, 1};

  std::unordered_map<GridlessMazeNodeHashVal, bool, GridlessMazeNodeHashFunc>
      vis;
  std::unordered_map<PII, GridlessMazeNode, pair_hash> pre[16];
  std::unordered_map<PII, bool, pair_hash> obsPoint;
  std::unordered_map<int, int> xMap, yMap;

  for (int i = 0; i < xList.size(); i++) {
    xMap[xList[i]] = i;
  }
  for (int i = 0; i < yList.size(); i++) {
    yMap[yList[i]] = i;
  }

  for (auto& [x, y] : sink) {
    FASSERT(xMap.contains(x))
      (x);
    FASSERT(yMap.contains(y))
      (y);
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
      int s1 = std::min(abs(b.xMin() - x1), abs(b.xMax() - x1));
      if (b.xMin() <= x1 && x1 <= b.xMax()) {
        s1 = 0;
      }
      int s2 = std::min(abs(b.yMin() - y1), abs(b.yMax() - y1));
      if (b.yMin() <= y1 && y1 <= b.yMax()) {
        s2 = 0;
      }
      a1 = std::min(a1, s1 + s2);
      a2 = std::min({a2, abs(x1 - b.xMin()), abs(x1 - b.xMax())});
    }
    return std::make_pair(a1, a2);
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

  auto check = [&, this](const GridlessMazeNode& cur, GridlessMazeNode& nex) {
    // 越界
    if (nex.x < 0 || nex.x >= xList.size() || nex.y < 0
        || nex.y >= yList.size()) {
      return false;
    }

    for (const auto& b : sinkBox) {
      if (b.contains(BsPoint(xList[nex.x], yList[nex.y]))) {
        switch (nex.preDir) {
          case WEST:
            nex.x = xMap[b.xMax()];
            break;
          case SOUTH:
            nex.y = yMap[b.yMax()];
            break;
          case EAST:
            nex.x = xMap[b.xMin()];
            break;
          case NORTH:
            nex.y = yMap[b.yMin()];
          default:
            break;
        }
        if (!checkOffGrid(xList[nex.x], yList[nex.y])) {
          return false;
        }
      }
    }

    // 二次进入起点
    if (!isInBoxesBoundary(xList[cur.x], yList[cur.y], sourceBox)
        && isInBoxes(xList[cur.x] + nextX[nex.preDir],
                     yList[cur.y] + nextY[nex.preDir],
                     sourceBox)) {
      return false;
    }

    if (!isInBoxes(xList[cur.x], yList[cur.y], sourceBox)
        && isInBoxes(xList[nex.x], yList[nex.y], sourceBox)) {
      return false;
    }

    // OBS and other pin
    if (!obsPoint.count(std::make_pair(xList[nex.x], yList[nex.y]))) {
      auto inObs = isInRtree(obsTree, xList[nex.x], yList[nex.y]);
      auto inOtherPin = isInRtree(pinTree, xList[nex.x], yList[nex.y]);
      if (inOtherPin || inObs) {
        obsPoint[std::make_pair(xList[nex.x], yList[nex.y])] = true;
      } else {
        obsPoint[std::make_pair(xList[nex.x], yList[nex.y])] = false;
      }
    }
    if (obsPoint[std::make_pair(xList[nex.x], yList[nex.y])]
        && !isInBoxes(xList[nex.x], yList[nex.y], sinkBox)
        && !isInBoxes(xList[nex.x], yList[nex.y], sourceBox)) {
      return false;
    }

    // 起点出去的时候
    if (isInBoxesBoundary(xList[cur.x], yList[cur.y], sourceBox)
        && !isInBoxesBoundary(xList[nex.x], yList[nex.y], sourceBox)) {
      // dis can bigger that GridU
      int dis = calcDis(cur.x, cur.y, nex.x, cur.y);
      if (!checkDrcInPin(xList[cur.x],
                         yList[cur.y],
                         invDir[nex.preDir],
                         sourceV,
                         sourceH,
                         false)
          || !checkOffGrid(xList[cur.x], yList[cur.y]) || dis > GridU_) {
        return false;
      }
    } else {
      if (isTurn(cur.preDir, nex.preDir)) {
        BsPoint cp(BsPoint(xList[cur.x], yList[cur.y]));
        if (!checkTurnPointDrc(cp, sourceBox)
            || !checkTurnPointDrc(cp, sinkBox)) {
          return false;
        }
      }
    }

    GmRoutingLine line{"",netName,
                       std::min(xList[cur.x], xList[nex.x]),
                       std::min(yList[cur.y], yList[nex.y]),
                       std::max(xList[cur.x], xList[nex.x]),
                       std::max(yList[cur.y], yList[nex.y])};
    if (!checkLineDrc(line, sourceV, sourceH)
        || !checkLineDrc(line, sinkV, sinkH)) {
      return false;
    }

    if (vis.count(std::make_tuple(nex.x, nex.y, nex.dirSta))) {
      return false;
    }

    return true;
  };

  std::vector<int> jumpGirdU[4];
  initBigger(jumpGirdU[EAST], xList, GridU_);
  initBigger(jumpGirdU[NORTH], yList, GridU_);
  initLess(jumpGirdU[WEST], xList, GridU_);
  initLess(jumpGirdU[SOUTH], yList, GridU_);
  std::vector<int> jumpFar[4];
  initBiggest(jumpFar[EAST], xList, 2 * U_ - 1);
  initBiggest(jumpFar[NORTH], yList, 2 * U_ - 1);
  initLeast(jumpFar[WEST], xList, 2 * U_ - 1);
  initLeast(jumpFar[SOUTH], yList, 2 * U_ - 1);

  std::priority_queue<GridlessMazeNode> q;
  for (const auto& [x, y] : source) {
    if (!isInBoxesBoundary(x, y, sourceBox) || !checkOffGrid(x, y)) {
      continue;
    }
    q.push({xMap[x], yMap[y], 0, calcDisToSink(xMap[x], yMap[y]), TOTDIR, -1});
    vis[std::make_tuple(xMap[x], yMap[y], TOTDIR)] = true;
  }

  GridlessMazeNode end{};
  end.dirSta = -1;
  int searchCount = 0;
  while (!q.empty() && end.dirSta == -1 && searchCount < searchLimit_) {
    auto cur = q.top();
    ++searchCount;
    q.pop();

    if (isInBoxes(xList[cur.x], yList[cur.y], sinkBox)) {
      if (checkDrcInPin(
              xList[cur.x], yList[cur.y], cur.preDir, sinkV, sinkH, true)) {
        end = cur;
        break;
      } else {
        continue;
      }
    }

    int preDir = cur.preDir;
    for (int i = 0; i < 4; i++) {
      bool turn = isTurn(preDir, i);
      if (canGo(cur.dirSta, i)) {
        auto k = cur;
        auto solveNewNode = [&]() {
          k.dirSta = 15;
          if (i != k.preDir) {
            k.preDir = i;
          }
          delDir(k.dirSta, {invDir[i]});
          if (check(cur, k)) {
            int dch = calcDis(k.x, k.y, cur.x, cur.y);
            k.dis += dch;
            if (isInBoxesBoundary(xList[k.x], yList[k.y], sourceBox)) {
              k.dis = 0;
            }
            k.weight = calcDisToSink(k.x, k.y);
            std::get<0>(k.weight) += k.dis;
            vis[std::make_tuple(k.x, k.y, k.dirSta)] = true;
            pre[k.dirSta][std::make_pair(k.x, k.y)] = cur;
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

        if (!checkOffGrid(xList[cur.x], yList[cur.y])) {
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

  std::vector<GmRoutingLine> res;
  if (end.dirSta == -1) {
    return res;
  }
  std::vector<std::pair<int, int>> pList;
  pList.emplace_back(end.x, end.y);
  while (pre[end.dirSta].count(std::make_pair(end.x, end.y))) {
    auto p = pre[end.dirSta][std::make_pair(end.x, end.y)];
    pList.emplace_back(p.x, p.y);
    if (isInBoxesBoundary(xList[p.x], yList[p.y], sourceBox)) {
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
    int x1 = std::min(pList[i].first, pList[j - 1].first);
    int x2 = std::max(pList[i].first, pList[j - 1].first);
    int y1 = std::min(pList[i].second, pList[j - 1].second);
    int y2 = std::max(pList[i].second, pList[j - 1].second);
    assert(checkOffGrid(xList[x1], yList[y1]));
    assert(checkOffGrid(xList[x2], yList[y2]));
    res.push_back({"", netName, xList[x1], yList[y1], xList[x2], yList[y2]});
    i = j - 2;
  }

  return res;
}
std::vector<ByBox> GridlessMaze::getRect(const std::vector<ByBox>& boxes,
                                         bsp::orientation_2d ori)
{
  BsPolygon90Set s;
  std::vector<ByBox> ans;
  for (const auto& b : boxes) {
    s.insert(b);
  }
  s.get_rectangles(ans, ori);
  return ans;
}
bool GridlessMaze::checkDrcInPin(int x,
                                 int y,
                                 int dir,
                                 const std::vector<ByBox>& vBoxes,
                                 const std::vector<ByBox>& hBoxes,
                                 bool isSink)
{
  int w = this->lineWidth_ / 2;
  if (dir == -1) {
    return false;
  }
  bool ok = false;
  auto solve = [&](const std::vector<ByBox>& boxes) {
    for (const auto& b : boxes) {
      int x1 = b.xMin();
      int x2 = b.xMax();
      int y1 = b.yMin();
      int y2 = b.yMax();
      if (x1 <= x && x <= x2 && y1 <= y && y <= y2) {
        if (dir == WEST || dir == EAST) {
          y1 += w;
          y2 -= w;
          if (!isSink) {
            if (dir == WEST) {
              x1 += w;
            } else {
              x2 -= w;
            }
          }
        } else {
          x1 += w;
          x2 -= w;
          if (!isSink) {
            if (dir == NORTH) {
              y2 -= w;
            } else {
              y1 += w;
            }
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
  if (dir == WEST || dir == EAST) {
    return solve(vBoxes);
  }
  return solve(hBoxes);
}
bool GridlessMaze::checkLineDrc(const GmRoutingLine& line,
                                const std::vector<ByBox>& vBoxes,
                                const std::vector<ByBox>& hBoxes)
{
  int U = this->U_;
  int lw = this->lineWidth_;
  int x1 = line.x1;
  int x2 = line.x2;
  int y1 = line.y1;
  int y2 = line.y2;

  auto lineIntersectLength = [&](int l1, int r1, int l2, int r2) {
    int l = std::max(l1, l2);
    int r = std::min(r1, r2);
    return std::max(0, r - l);
  };

  auto checkLevel = [&](const ByBox& b) {
    if (b.contains(BsPoint(x1, y1)) || b.contains(BsPoint(x2, y2))) {
      return true;
    }
    if (lineIntersectLength(x1, x2, b.xMin(), b.xMax()) > 0) {
      if ((abs(b.yMin() - y1) < U && abs(b.yMin() - y1) > lw / 2)
          || (abs(b.yMax() - y1) < U && abs(b.yMax() - y1) > lw / 2)) {
        return false;
      }
    }
    return true;
  };

  auto checkVertical = [&](const ByBox& b) {
    if (b.contains(BsPoint(x1, y1)) || b.contains(BsPoint(x2, y2))) {
      return true;
    }
    if (lineIntersectLength(y1, y2, b.yMin(), b.yMax())) {
      if ((abs(b.xMin() - x1) < U && abs(b.xMin() - x1) > lw / 2)
          || (abs(b.xMax() - x1) < U && abs(b.xMax() - x1) > lw / 2)) {
        return false;
      }
    }
    return true;
  };

  bool isLevel = (y1 == y2);
  if (isLevel) {
    x1 -= lw / 2;
    x2 += lw / 2;
  } else {
    y1 -= lw / 2;
    y2 += lw / 2;
  }

  auto solve = [&](const std::vector<ByBox>& boxes) {
    for (const auto& b : boxes) {
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
  if (isLevel) {
    return solve(vBoxes);
  }
  return solve(hBoxes);
}
bool GridlessMaze::isInBoxes(int x,
                             int y,
                             const std::vector<ByBox>& boxes,
                             bool touch)
{
  for (const auto& b : boxes) {
    if (b.contains(BsPoint(x, y), touch)) {
      return true;
    }
  }
  return false;
}
bool GridlessMaze::isInBoxesBoundary(int x,
                                     int y,
                                     const std::vector<ByBox>& boxes)
{
  for (const auto& b : boxes) {
    if (x == b.xMin() || x == b.xMax()) {
      if (b.yMin() <= y && y <= b.yMax()) {
        return true;
      }
    }
    if (y == b.yMin() || y == b.yMax()) {
      if (b.xMin() <= x && x <= b.xMax()) {
        return true;
      }
    }
  }
  return false;
}
bool GridlessMaze::checkOffGrid(int x, int y)
{
  if (x % manufacturingGrid_ || y % manufacturingGrid_) {
    return false;
  }
  return true;
}
bool GridlessMaze::isInRtree(const GridlessMaze::Rtree& rt, int x, int y)
{
  ByBox b(x, y, x, y);
  std::vector<ByBox> ans;
  rt.query(bgi::intersects(b), std::back_inserter(ans));
  return !ans.empty();
}
void GridlessMaze::initBigger(std::vector<int>& v,
                              const std::vector<int>& a,
                              int x)
{
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
}
void GridlessMaze::initLess(std::vector<int>& v,
                            const std::vector<int>& a,
                            int x)
{
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
}
void GridlessMaze::initBiggest(std::vector<int>& v,
                               const std::vector<int>& a,
                               int x)
{
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
}
void GridlessMaze::initLeast(std::vector<int>& v,
                             const std::vector<int>& a,
                             int x)
{
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
}
std::vector<ByBox> GridlessMaze::getMaxRect(const std::vector<ByBox>& boxes)
{
  BsPolygon90Set s;
  for (const auto& b : boxes) {
    s += b;
  }
  std::vector<ByBox> ans;
  bsp::get_max_rectangles(ans, s);
  return ans;
}
bool GridlessMaze::checkTurnPointDrc(const BsPoint& p,
                                     const std::vector<ByBox>& boxes)
{
  ByBox b(p.x(), p.y(), p.x(), p.y());
  b.expand(lineWidth_ / 2);
  for (const auto& tb : boxes) {
    auto eb = tb;
    eb.expand(U_ - 1);
    if (!bsp::intersects(tb, b, true) && eb.contains(p)) {
      return false;
    }
  }
  return true;
}

}  // namespace banyan
