#include "PinCluster.hpp"

#include <algorithm>
#include <boost/assign.hpp>
#include <boost/foreach.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "thread_pool/ThreadPool.h"
#include "utl/Logger.h"

namespace banyan {

GmBox GmBox::GetAbsoluteGMBox(int xMin,
                              int yMin,
                              int xMax,
                              int yMax,
                              const odb::dbTransform& transformer,
                              odb::dbTechLayer* layer,
                              std::string name,
                              Type type)
{
  odb::Rect rect(xMin, yMin, xMax, yMax);
  transformer.apply(rect);
  return {rect.xMin(),
          rect.yMin(),
          rect.xMax(),
          rect.yMax(),
          layer,
          std::move(name),
          type};
}

GmBox::GmBox(const odb::Rect& r,
             odb::dbTechLayer* layer,
             std::string name,
             Type type)
    : layer(layer), type(type), name(std::move(name))
{
  xMin = r.xMin();
  xMax = r.xMax();
  yMin = r.yMin();
  yMax = r.yMax();
}

std::string PinClusterBuilder::PinName(odb::dbITerm* dbIterm)
{
  auto inst = dbIterm->getInst();
  auto instName = inst->getName();
  auto mTerm = dbIterm->getMTerm();
  auto mName = mTerm->getName();
  auto pinName = instName + "@" + mName;
  return pinName;
}

std::string PinClusterBuilder::PinNameInst(std::string s)
{
  std::string ans = "";
  for (auto& i : s) {
    if (i == '@') {
      break;
    }
    ans += i;
  }
  return ans;
}

std::string PinClusterBuilder::PinNamePin(std::string s)
{
  std::string ans = "";
  bool ok = false;
  for (auto& i : s) {
    if (ok) {
      ans += i;
    }
    if (i == '@') {
      ok = true;
    }
  }
  return ans;
}

odb::dbTechLayer* PinClusterBuilder::FindFirstRoutingLayer(odb::dbTech* dbTech)
{
  for (auto layer : dbTech->getLayers()) {
    if (layer->getType() == odb::dbTechLayerType::ROUTING) {
      return layer;
    }
  }
  return nullptr;
}

std::vector<PinCluster> PinClusterBuilder::Build()
{
  auto chip = db_->getChip();
  auto block = chip->getBlock();
  auto fLayer = FindFirstRoutingLayer(db_->getTech());

  odb::Rect gmBoxRect;
  block->getCoreArea(gmBoxRect);
  GmBox boundBox = GmBox(gmBoxRect.xMin(),
                         gmBoxRect.yMin(),
                         gmBoxRect.xMax(),
                         gmBoxRect.yMax(),
                         fLayer);
  logger_->info(utl::BY,
                1,
                "bbox:{},{},{},{}",
                gmBoxRect.xMin(),
                gmBoxRect.yMin(),
                gmBoxRect.xMax(),
                gmBoxRect.yMax());
  std::unordered_map<std::string, std::string> pin2net;
  std::unordered_map<std::string, std::vector<std::string>> net2pins;
  logger_->info(utl::BY, 1, "instSize: {}", block->getInsts().size());
  for (auto net : block->getNets()) {
    if (net->isSpecial()) {
      continue;
    }
    auto netName = net->getName();
    for (auto pin : net->getITerms()) {
      auto pinName = PinName(pin);
      pin2net[pinName] = netName;
      net2pins[netName].push_back(pinName);
    }
  }
  std::vector<GmBox> blockages, pows, pinBoxes;
  std::vector<GmPin> pins;
  for (auto pow : block->getNets()) {
    if (pow->isSpecial()) {
      for (auto sw : pow->getSWires()) {
        for (auto odbSBox : sw->getWires()) {
          if (odbSBox->isVia()) {
            int x;
            int y;
            std::vector<odb::dbShape> vBoxes;
            odbSBox->getViaXY(x, y);
            odbSBox->getViaBoxes(vBoxes);
            for (const auto& box : vBoxes) {
              GmBox b(box.xMin(), box.yMin(), box.xMax(), box.yMax(), fLayer);
              if (box.getTechLayer() == fLayer && boundBox.intersect(b)) {
                blockages.push_back(b);
              }
            }
          } else {
            int x1 = odbSBox->xMin();
            int y1 = odbSBox->yMin();
            int x2 = odbSBox->xMax();
            int y2 = odbSBox->yMax();
            GmBox b(x1, y1, x2, y2, fLayer);
            if (odbSBox->getTechLayer() == fLayer && boundBox.intersect(b)) {
              pows.push_back(b);
            }
          }
        }
      }
    }
  }

  for (auto obs : block->getObstructions()) {
    auto box = obs->getBBox();
    GmBox b(box->xMin(), box->yMin(), box->xMax(), box->yMax(), fLayer);
    if (box->getTechLayer() == fLayer && boundBox.intersect(b)) {
      blockages.push_back(b);
    }
  }

  for (auto inst : block->getInsts()) {
    int x = 0, y = 0;
    inst->getOrigin(x, y);
    odb::dbTransform transformer(inst->getOrient(), odb::Point(x, y));
    auto master = inst->getMaster();
    // inst obs
    for (auto odbSBox : master->getObstructions()) {
      if (odbSBox->isVia()) {
        int vx;
        int vy;
        std::vector<odb::dbShape> vBoxes;
        odbSBox->getViaXY(vx, vy);
        odbSBox->getViaBoxes(vBoxes);
        for (const auto& box : vBoxes) {
          auto b = GmBox::GetAbsoluteGMBox(box.xMin(),
                                           box.yMin(),
                                           box.xMax(),
                                           box.yMax(),
                                           transformer,
                                           fLayer);
          if (box.getTechLayer() == fLayer && boundBox.intersect(b)) {
            blockages.push_back(b);
          }
        }
      } else {
        int x1 = odbSBox->xMin();
        int y1 = odbSBox->yMin();
        int x2 = odbSBox->xMax();
        int y2 = odbSBox->yMax();
        auto b = GmBox::GetAbsoluteGMBox(x1, y1, x2, y2, transformer, fLayer);
        if (odbSBox->getTechLayer() == fLayer && boundBox.intersect(b)) {
          blockages.push_back(b);
        }
      }
    }
    // inst pin
    for (auto mTerm : master->getMTerms()) {
      auto pinName = inst->getName() + "@" + mTerm->getName();
      if (pin2net.count(pinName)) {
        for (auto mpin : mTerm->getMPins()) {
          for (auto box : mpin->getGeometry()) {
            auto b = GmBox::GetAbsoluteGMBox(box->xMin(),
                                             box->yMin(),
                                             box->xMax(),
                                             box->yMax(),
                                             transformer,
                                             fLayer);
            b.type = GmBox::PIN;
            b.name = pinName;
            if (box->getTechLayer() == fLayer && boundBox.intersect(b)) {
              pinBoxes.push_back(b);
            }
          }
        }
      } else {
        for (auto mpin : mTerm->getMPins()) {
          for (auto box : mpin->getGeometry()) {
            auto b = GmBox::GetAbsoluteGMBox(box->xMin(),
                                             box->yMin(),
                                             box->xMax(),
                                             box->yMax(),
                                             transformer,
                                             fLayer);
            b.type = GmBox::OBS;
            b.name = pinName;
            if (box->getTechLayer() == fLayer && boundBox.intersect(b)) {
              blockages.push_back(b);
            }
          }
        }
      }
    }
  }

  auto rows = GetRow(boundBox, pows);
  auto totBoxes = blockages;
  logger_->info(utl::BY, 1, "blockages:{} ", totBoxes.size());
  totBoxes.insert(totBoxes.end(), pinBoxes.begin(), pinBoxes.end());
  time_t stTime = time(nullptr);
  auto rowBoxes = GetRowBoxes(rows, totBoxes);
  logger_->info(
      utl::BY, 1, "GetRowBoxes time:{}", (double) (time(nullptr) - stTime));
  int spacing = fLayer->getSpacing() + fLayer->getWidth() / 2;
  // expand  -1:边界可以走
  for (auto& row : rowBoxes) {
    for (auto& b : row) {
      if (b.type == GmBox::OBS) {
        b.expand(spacing - 1);
      }
    }
  }
  // narrow  不-1，因为row的判断条件是 yMin<=y<=yMax,边界走了
  for (auto& row : rows) {
    row.narrowNS(spacing);
  }

  // 获取当前系统的可用CPU核数
  int enableCPUNum = sysconf(_SC_NPROCESSORS_ONLN);
  ThreadPool pool((enableCPUNum - 1) * 2);
  std::vector<PinCluster> pinClusters;
  std::vector<std::future<std::vector<PinCluster>>> futurePinClusters;
  for (int i = 0; i < rows.size(); i++) {
    futurePinClusters.emplace_back(pool.enqueue(
        [this](const GmBox& row, const std::vector<GmBox>& rowBoxes) {
          return this->GetPinClusters(row, rowBoxes);
        },
        std::ref(rows[i]),
        std::ref(rowBoxes[i])));
  }
  for (auto& f : futurePinClusters) {
    auto res = f.get();
    pinClusters.insert(pinClusters.end(), res.begin(), res.end());
  }

  return pinClusters;
}

std::vector<std::vector<GmBox>> PinClusterBuilder::GetRowBoxes(
    const std::vector<GmBox>& rows,
    const std::vector<GmBox>& boxes)
{
  std::vector<std::vector<GmBox>> ans(rows.size());
  namespace bg = boost::geometry;
  namespace bgi = boost::geometry::index;
  typedef bg::model::d2::point_xy<int, boost::geometry::cs::cartesian> DPoint;
  typedef bg::model::box<DPoint> DBox;
  typedef std::pair<DBox, unsigned> Value;
  bgi::rtree<Value, bgi::quadratic<100000>>
      rt;  // 采用quadratic algorithm，节点中元素个数最多1e5个

  std::vector<GmPointEvent> events;
  for (int i = 0; i < rows.size(); i++) {
    auto b = rows[i];
    events.emplace_back(b.xMin, b.yMin, i, GmPointEvent::IN);
    events.emplace_back(b.xMin, b.yMax, i, GmPointEvent::OUT);
  }
  for (int i = 0; i < boxes.size(); i++) {
    auto b = boxes[i];
    events.emplace_back(b.xMin, b.yMin, i + rows.size(), GmPointEvent::IN);
    events.emplace_back(b.xMin, b.yMax, i + rows.size(), GmPointEvent::OUT);
  }
  std::sort(events.begin(),
            events.end(),
            [](const GmPointEvent& lhs, const GmPointEvent& rhs) {
              return lhs.y < rhs.y;
            });

  auto getDbox = [](const std::vector<GmBox>& box, int i) {
    auto b = box[i];
    return DBox(DPoint(b.xMin, b.yMin), DPoint(b.xMax, b.yMax));
  };
  std::vector<std::unordered_set<int>> bel(boxes.size());
  for (int i = 0; i < events.size(); i++) {
    int j = i;
    while (j < events.size() && events[j].y == events[i].y) {
      ++j;
    }
    for (int k = i; k < j; k++) {
      auto e = events[k];
      if (e.id >= rows.size() && e.s == GmPointEvent::OUT) {
        int id = e.id - rows.size();
        std::vector<Value> res;
        auto db = getDbox(boxes, id);
        rt.query(bgi::intersects(db), std::back_inserter(res));
        BOOST_FOREACH (Value const& v, res) {
          if (!bel[id].count(v.second)) {
            ans[v.second].push_back(boxes[id]);
            bel[id].insert(v.second);
          }
        }
      }
    }
    for (int k = i; k < j; k++) {
      auto e = events[k];
      if (e.id < rows.size()) {
        if (e.s == GmPointEvent::IN) {
          rt.insert(std::make_pair(getDbox(rows, e.id), e.id));
        } else {
          rt.remove(std::make_pair(getDbox(rows, e.id), e.id));
        }
      }
    }
    for (int k = i; k < j; k++) {
      auto e = events[k];
      if (e.id >= rows.size() && e.s == GmPointEvent::IN) {
        int id = e.id - rows.size();
        std::vector<Value> res;
        auto db = getDbox(boxes, id);
        rt.query(bgi::intersects(db), std::back_inserter(res));
        BOOST_FOREACH (Value const& v, res) {
          if (!bel[id].count(v.second)) {
            ans[v.second].push_back(boxes[id]);
            bel[id].insert(v.second);
          }
        }
      }
    }
    i = j - 1;
  }
  for (auto& row : ans) {
    std::sort(row.begin(), row.end(), [](const GmBox& lhs, const GmBox& rhs) {
      return lhs.xMax < rhs.xMax;
    });
  }
  return ans;
}

std::vector<GmBox> PinClusterBuilder::GetRow(const GmBox& mb,
                                             std::vector<GmBox> pows)
{
  std::vector<GmBox> ans;
  std::vector<GmLine> lines;

  std::set<GmBox> uselessBox;
  lines.emplace_back(mb.xMin, mb.yMax, mb.xMax, mb.yMax);
  lines.emplace_back(mb.xMin, mb.yMin, mb.xMax, mb.yMin);

  auto mergeBox = [](const std::vector<GmBox>& boxes) {
    std::map<std::pair<int, int>, std::vector<GmBox>> mergeMap;
    for (const auto& b : boxes) {
      mergeMap[std::make_pair(b.yMin, b.yMax)].emplace_back(b);
    }
    std::vector<GmBox> ans;
    for (auto& [x, y] : mergeMap) {
      std::sort(y.begin(), y.end(), [](const GmBox& lhs, const GmBox& rhs) {
        return lhs.xMin < rhs.xMin;
      });
      int lx = y[0].xMin;
      int rx = y[0].xMax;
      for (int i = 1; i < y.size(); i++) {
        if (y[i].xMin <= rx) {
          rx = std::max(rx, y[i].xMax);
        } else {
          ans.emplace_back(GmBox(lx, x.first, rx, x.second));
          lx = y[i].xMin;
          rx = y[i].xMax;
        }
      }
      ans.emplace_back(GmBox(lx, x.first, rx, x.second));
    }
    return ans;
  };

  pows = mergeBox(pows);

  for (const auto& b : pows) {
    lines.emplace_back(b.xMin, b.yMax, b.xMax, b.yMax);
    lines.emplace_back(b.xMin, b.yMin, b.xMax, b.yMin);
    uselessBox.insert(b);
  }

  std::map<std::pair<int, int>, std::vector<int>> verLine;
  std::vector<GmPointEvent> events;
  for (int i = 0; i < lines.size(); i++) {
    auto w = lines[i];
    events.emplace_back(w.lx, w.ly, i, GmPointEvent::IN);
    events.emplace_back(w.rx, w.ry, i, GmPointEvent::OUT);
  }
  std::sort(events.begin(),
            events.end(),
            [](const GmPointEvent& lhs, const GmPointEvent& rhs) {
              if (lhs.x == rhs.x)
                return lhs.y < rhs.y;
              return lhs.x < rhs.x;
            });

  std::set<GmPointEvent> s;

  auto getPre = [&](const GmPointEvent& x) {
    auto it = s.lower_bound(x);
    if (it == s.begin())
      return s.end();
    --it;
    return it;
  };
  auto getSuf = [&](const GmPointEvent& x) {
    auto it = s.upper_bound(x);
    return it;
  };

  auto getEventIn = [&](const GmPointEvent& x) {
    int id = x.id;
    auto w = lines[id];
    return GmPointEvent(w.lx, w.ly, id, GmPointEvent::IN);
  };

  auto removeIn = [&](const GmPointEvent& x) { s.erase(getEventIn(x)); };

  auto matchLine = [&](std::pair<int, int> val, int xMax) {
    if (!verLine[val].empty()) {
      auto xMin = verLine[val].back();
      verLine[val].pop_back();
      int yMin = val.first;
      int yMax = val.second;
      GmBox b(xMin, yMin, xMax, yMax);
      if (!uselessBox.count(b) && xMax != xMin) {
        ans.push_back(b);
      }
      return true;
    }
    return false;
  };

  for (int i = 0; i < events.size(); i++) {
    int j = i;
    while (j < events.size() && events[j].x == events[i].x) {
      ++j;
    }
    std::vector<int> in, out;
    for (int k = i; k < j; k++) {
      if (events[k].s == GmPointEvent::IN) {
        in.emplace_back(k);
      } else {
        out.emplace_back(k);
      }
    }

    auto getQl = [&](int k) {
      auto cp = events[k];
      auto pre = getPre(cp);
      auto suf = getSuf(cp);
      std::vector<int> ql;
      if (pre != s.end()) {
        ql.push_back((*pre).y);
      }
      ql.push_back(cp.y);
      if (suf != s.end()) {
        ql.push_back((*suf).y);
      }
      return ql;
    };

    for (auto k : out) {
      auto& cp = events[k];
      auto ql = getQl(k);
      for (int lo = 1; lo < ql.size(); lo++) {
        std::pair<int, int> val(ql[lo - 1], ql[lo]);
        matchLine(val, cp.x);
      }
    }
    for (auto k : out) {
      removeIn(events[k]);
    }
    for (auto k : out) {
      auto& cp = events[k];
      auto ql = getQl(k);
      std::pair<int, int> val(ql.front(), ql.back());
      if (!verLine[val].empty() && verLine[val].back() == cp.x) {
        continue;
      }
      verLine[val].emplace_back(cp.x);
    }
    for (auto k : in) {
      auto& cp = events[k];
      auto ql = getQl(k);
      std::pair<int, int> val(ql.front(), ql.back());
      matchLine(val, cp.x);
    }
    for (auto k : in) {
      s.insert(events[k]);
    }
    for (auto k : in) {
      auto& cp = events[k];
      auto ql = getQl(k);
      for (int lo = 1; lo < ql.size(); lo++) {
        std::pair<int, int> val(ql[lo - 1], ql[lo]);
        if (!verLine[val].empty() && verLine[val].back() == cp.x) {
          continue;
        }
        verLine[val].push_back(cp.x);
      }
    }
    i = j - 1;
  }
  ans = mergeBox(ans);
  sort(ans.begin(), ans.end());
  return ans;
}

std::vector<PinCluster> PinClusterBuilder::GetPinClusters(
    const GmBox& row,
    const std::vector<GmBox>& rowBoxes)
{
  namespace bg = boost::geometry;
  namespace bgi = boost::geometry::index;
  typedef bg::model::d2::point_xy<int, boost::geometry::cs::cartesian> DPoint;
  typedef bg::model::box<DPoint> DBox;
  typedef std::pair<DBox, unsigned> Value;
  bgi::rtree<Value, bgi::quadratic<100000>>
      rt;  // 采用quadratic algorithm，节点中元素个数最多1e5个

  std::vector<PinCluster> ans;

  auto getDbox = [](const GmBox& b) {
    return DBox(DPoint(b.xMin, b.yMin), DPoint(b.xMax, b.yMax));
  };

  std::unordered_map<std::string, std::vector<int>> instMap;
  for (int i = 0; i < rowBoxes.size(); i++) {
    auto iName = PinNameInst(rowBoxes[i].name);
    if (iName != "") {
      instMap[iName].emplace_back(i);
    }
    rt.insert(std::make_pair(getDbox(rowBoxes[i]), i));
  }

  auto solve = [&](const GmBox& bbox) {
    std::vector<Value> res;
    auto db = getDbox(bbox);
    std::vector<GmBox> ans;
    rt.query(bgi::intersects(db), std::back_inserter(res));
    BOOST_FOREACH (Value const& v, res) {
      ans.emplace_back(rowBoxes[v.second]);
    }
    return ans;
  };
  std::vector<std::pair<int, int>> v;

  for (auto& [iName, bids] : instMap) {
    int x1 = std::numeric_limits<int>::max();
    int x2 = std::numeric_limits<int>::min();
    for (auto& id : bids) {
      const auto& b = rowBoxes[id];
      x1 = std::min(x1, b.xMin);
      x2 = std::max(x2, b.xMax);
    }
    v.emplace_back(std::make_pair(x1, x2));
  }
  std::sort(v.begin(), v.end());
  int len = v.size();
  for (int i = 0; i < len - 2; i += 2) {
    int x1 = std::min(v[i].first, v[i + 2].first);
    int x2 = std::max(v[i].second, v[i + 2].second);
    ans.emplace_back(
        x1, row.yMin, x2, row.yMax, solve(GmBox(x1, row.yMin, x2, row.yMax)));
  }

  return ans;
}
PinClusterBuilder::polygon PinClusterBuilder::GetRectPoly(int x1,
                                                          int y1,
                                                          int x2,
                                                          int y2)
{
  polygon poly;
  point pts[5] = {boost::polygon::construct<point>(x1, y1),
                  boost::polygon::construct<point>(x1, y2),
                  boost::polygon::construct<point>(x2, y2),
                  boost::polygon::construct<point>(x2, y1),
                  boost::polygon::construct<point>(x1, y1)};
  boost::polygon::set_points(poly, pts, pts + 5);
  return poly;
}
std::vector<PinClusterBuilder::polygon> PinClusterBuilder::GetObsPolyGon(
    const std::vector<GmBox>& rows,
    const std::vector<std::vector<GmBox>>& rowBoxes)
{
  logger_->info(
      utl::BY, 1, "-------------------GetObsPolyGon running---------------");

  std::vector<polygon> ans;
  for (int i = 0; i < rowBoxes.size(); i++) {
    auto row = rowBoxes[i];
    polygon_set bound;
    bound.insert(
        GetRectPoly(rows[i].xMin, rows[i].yMin, rows[i].xMax, rows[i].yMax));
    polygon_set s;
    for (const auto& box : row) {
      auto b = box;
      if (b.type == GmBox::PIN) {
        continue;
      }
      s.insert(GetRectPoly(b.xMin, b.yMin, b.xMax, b.yMax));
    }
    bound -= s;
    std::vector<polygon> res;
    s.get<std::vector<polygon>>(res);
    ans.insert(ans.end(), res.begin(), res.end());
  }
  logger_->info(
      utl::BY, 1, "-------------------GetObsPolyGon done---------------");

  return ans;
}

}  // namespace banyan