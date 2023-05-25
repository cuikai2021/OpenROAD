#include "ViaCandidate.hpp"

#include "GridlessMaze.hpp"
namespace banyan {

std::vector<std::shared_ptr<ViaCandidate>> ViaCandidateBuilder::run()
{
  auto tech = db_->getTech();
  m1_ = ByAdapter::findFirstRoutingLayer(tech);
  logger_->info(utl::BY, 1, "{}", m1_->getRoutingLevel());
  cut1_ = ByAdapter::findNextCutLayer(tech, m1_);
  m2_ = ByAdapter::findNextRoutingLayer(tech, m1_);
  auto flBoxes = bydb_->getLayerBoxes({m1_->getName(), cut1_->getName()});

  std::unordered_map<std::string, std::vector<InstPinName>> vpin2pins;
  for (int i = 0; i < vpins.size(); i++) {
    std::string vpinName = "vpin" + std::to_string(i);
    vpin2pins[vpinName] = vpins[i].names;
    auto info = std::make_shared<ByBoxInfo>(m1_->getName(),
                                            vpins[i].netName,
                                            std::nullopt,
                                            vpinName,
                                            std::nullopt,
                                            ByBoxInfo::INST_PIN);

    for (const auto& line : vpins[i].lines) {
      auto b = line.toBox(m1_->getWidth());
      flBoxes.emplace_back(std::make_shared<ByBox>(
          b.xMin(), b.yMin(), b.xMax(), b.yMax(), info));
    }
  }

  auto rows = bydb_->getRows();
  logger_->info(utl::BY, 1, "fl:{}", flBoxes.size());
  std::vector<std::vector<std::shared_ptr<ByBox>>> rowBoxes;
  ByDb::getRowBoxes(rowBoxes, rows, flBoxes);

  auto block = db_->getChip()->getBlock();
  if (m1_->getDirection() == odb::dbTechLayerDir::VERTICAL) {
    logger_->info(utl::BY, 1, "m1 VERTICAL");
    block->findTrackGrid(m1_)->getGridX(gx_);
    block->findTrackGrid(m2_)->getGridY(gy_);
  } else {
    logger_->info(utl::BY, 1, "m1 HORIZONTAL");
    block->findTrackGrid(m1_)->getGridY(gy_);
    block->findTrackGrid(m2_)->getGridX(gx_);
  }

  m1Spacing_ = m1_->getSpacing();
  m1Spacing_ = std::max(m1Spacing_, ByAdapter::getEndOfLineSpacing(m1_));

  std::vector<std::shared_ptr<ViaCandidate>> ans;

  time_t st = time(nullptr);

  int enableCPUNum = sysconf(_SC_NPROCESSORS_ONLN);
  ThreadPool pool((enableCPUNum - 1) * 2);
  std::vector<std::future<std::vector<std::shared_ptr<ViaCandidate>>>> fRes;
  for (int i = 0; i < rows.size(); i++) {
    fRes.emplace_back(pool.enqueue(
        [this](const std::shared_ptr<ByBox>& row,
               const std::vector<std::shared_ptr<ByBox>>& boxes) {
          return this->solveRow(row, boxes);
        },
        std::ref(rows[i]),
        std::ref(rowBoxes[i])));
  }

  for (auto& w : fRes) {
    auto res = w.get();
    ans.insert(ans.end(), res.begin(), res.end());
  }

  auto castTime = time(nullptr) - st;

  for (int i = 0; i < ans.size(); i += 100) {
    auto net = bydb_->findNet(ans[i]->getNetName());
    net->mutableVias().emplace_back(ans[i]->getVia());
    for (const auto& b : ans[i]->getWires()) {
      net->mutableWires().emplace_back(std::make_shared<ByBox>(b));
    }
  }

  for (const auto& x : vpins) {
    for (auto& line : x.lines) {
      bydb_->findNet(x.netName)->mutableWires().emplace_back(
          std::make_shared<ByBox>(line.toBox(m1_->getWidth())));
    }
  }
  ByAdapter::updateWire(db_, bydb_);

  logger_->info(utl::BY, 1, "time:{}s  viaCandidate:{} ", castTime, ans.size());
  return ans;
}
std::vector<std::shared_ptr<ViaCandidate>> ViaCandidateBuilder::solveRow(
    const std::shared_ptr<ByBox>& row,
    const std::vector<std::shared_ptr<ByBox>>& boxes)
{
  std::vector<std::shared_ptr<ViaCandidate>> ans;

  std::vector<std::shared_ptr<ByBox>> fixed[2];
  for (const auto& b : boxes) {
    if (b->getLayerName() == m1_->getName()) {
      fixed[0].emplace_back(b);
    } else {
      fixed[1].emplace_back(b);
    }
  }

  auto checkVia = [&](const std::shared_ptr<ByVia>& via) {
    if (via->getTopLayer()->getName() != m2_->getName()) {
      return false;
    }
    for (const auto& b : via->getBoxes()) {
      if (!strcmp(b->getLayerNameCStr(), m2_->getConstName())) {
        if (m2_->getDirection() == odb::dbTechLayerDir::VERTICAL) {
          return b->dx() <= m2_->getWidth();
        } else {
          return b->dy() <= m2_->getWidth();
        }
      }
    }
    return false;
  };

  std::vector<int> xList;
  std::vector<int> yList;
  xList.insert(xList.end(),
               std::lower_bound(gx_.begin(), gx_.end(), row->xMin()),
               std::upper_bound(gx_.begin(), gx_.end(), row->xMax()));
  yList.insert(yList.end(),
               std::lower_bound(gy_.begin(), gy_.end(), row->yMin()),
               std::upper_bound(gy_.begin(), gy_.end(), row->yMax()));

  std::vector<std::shared_ptr<ViaCandidate>> tmpRes;
  std::vector<std::shared_ptr<ByVia>> m2Vias;
  for (const auto& via : bydb_->getVias()) {
    if (checkVia(via)) {
      m2Vias.emplace_back(via);
    }
  }
  std::set<std::pair<int, int>> gpSet;
  for (const auto& b : boxes) {
    if (b->getType() == ByBoxInfo::INST_PIN
        || b->getType() == ByBoxInfo::WIRE) {
      auto ps = getGridPoint(xList, yList, b);
      for (auto& [x, y] : ps) {
        if (gpSet.count({x, y})) {
          continue;
        }
        gpSet.insert({x, y});
        for (const auto& via : m2Vias) {
          auto viaInst = std::make_shared<ByViaInst>(
              via, via->getTopLayer(), b->getNetName(), BsPoint(x, y));
          getPinCandidate(tmpRes, *b, viaInst);
        }
      }
    }
  }

  std::vector<int> vis(tmpRes.size());

  auto cmp = [&](const std::shared_ptr<ByBox>& lhs,
                 const std::shared_ptr<ByBox>& rhs) {
    return lhs->xMin() < rhs->xMin();
  };
  using Value = std::pair<ByBox, int>;
  std::vector<Value> routed[2];
  for (int i = 0; i < tmpRes.size(); i++) {
    routed[0].emplace_back(tmpRes[i]->getVia()->getBottomBox(), i);
    for (const auto& b : tmpRes[i]->getVia()->getCutBoxes()) {
      routed[1].emplace_back(b, i);
    }
    for (const auto& b : tmpRes[i]->getWires()) {
      routed[0].emplace_back(b, i);
    }
  }

  auto solve = [&](std::vector<std::shared_ptr<ByBox>>& fixed,
                   std::vector<Value>& routed) {
    std::sort(fixed.begin(), fixed.end(), cmp);
    std::sort(
        routed.begin(), routed.end(), [&](const Value& lhs, const Value& rhs) {
          return lhs.first.xMin() < rhs.first.xMin();
        });
    std::vector<int> active;
    int j = 0;
    for (const auto& [b, id] : routed) {
      if (vis[id]) {
        continue;
      }
      ByBox region(b.xMin() - m1Spacing_,
                   row->yMin(),
                   b.xMax() + m1Spacing_,
                   row->yMax());
      while (j < fixed.size() && fixed[j]->xMin() <= region.xMax()) {
        if (region.intersects(*fixed[j])) {
          active.emplace_back(j);
        }
        ++j;
      }
      std::vector<int> tmpActive;
      bool ok = true;
      for (const auto& x : active) {
        const auto& tb = fixed[x];
        if (tb->xMax() <= region.xMin()) {
          continue;
        }
        tmpActive.emplace_back(x);
        if (ok && !checkDRC(b, *tb)) {
          ok = false;
        }
      }
      if (!ok) {
        vis[id] = 1;
      }
      active.swap(tmpActive);
    }
  };

  solve(fixed[1], routed[1]);
  solve(fixed[0], routed[0]);

  for (int i = 0; i < vis.size(); i++) {
    if (!vis[i]) {
      ans.emplace_back(tmpRes[i]);
    }
  }

  return ans;
}
std::vector<std::pair<int, int>> ViaCandidateBuilder::getGridPoint(
    const std::vector<int>& vx,
    const std::vector<int>& vy,
    const std::shared_ptr<ByBox>& b)
{
  auto lower = [&](const std::vector<int>& a, int x) {
    return *std::lower_bound(a.begin(), a.end(), x);
  };

  auto less = [&](const std::vector<int>& a, int x) {
    auto it = std::upper_bound(a.begin(), a.end(), x);
    if (it != a.begin()) {
      --it;
    }
    return *it;
  };

  auto initList
      = [&](std::vector<int>& v, const std::vector<int>& a, int l, int r) {
          auto it = std::lower_bound(a.begin(), a.end(), l);
          while (it != a.end()) {
            if (*it > r) {
              return;
            }
            v.emplace_back(*it);
            it++;
          }
        };

  std::vector<std::pair<int, int>> ans;
  std::vector<int> xList;
  std::vector<int> yList;

  initList(xList, vx, b->xMin(), b->xMax());
  initList(yList, vy, b->yMin(), b->yMax());

  if (xList.empty()) {
    int r = lower(vx, b->xMax());
    int l = less(vx, b->xMin());
    int cr = abs(r - b->xMax());
    int cl = abs(l - b->xMin());
    int m = std::min(cl, cr);
    if (cr == m) {
      xList.emplace_back(r);
    }
    if (cl == m) {
      xList.emplace_back(l);
    }
  }
  if (yList.empty()) {
    int r = lower(vy, b->yMax());
    int l = less(vy, b->yMin());
    int cr = abs(r - b->yMax());
    int cl = abs(l - b->yMin());
    int m = std::min(cl, cr);
    if (cr == m) {
      yList.emplace_back(r);
    }
    if (cl == m) {
      yList.emplace_back(l);
    }
  }

  for (auto x : xList) {
    for (auto y : yList) {
      ans.emplace_back(x, y);
    }
  }

  return ans;
}
// TODO too simple DRC check, need more
bool ViaCandidateBuilder::checkDRC(const ByBox& a, const ByBox& b)
{
  if (!strcmp(a.getLayerNameCStr(), cut1_->getConstName())) {
    if (a.intersects(b)) {
      return false;
    }
    return checkPRL(a, b);
  }
  if (a.intersects(b)) {
    if (!strcmp(a.getNetName().c_str(), b.getNetName().c_str())) {
      auto ch = ByBox::getIntersection(a, b);
      return ch.getDiagLenSquared()
             >= (size_t) m1_->getMinWidth() * m1_->getMinWidth();
    } else {
      return false;
    }
  }
  return checkPRL(a, b);
}
bool ViaCandidateBuilder::checkPRL(const ByBox& a, const ByBox& b)
{
  ByBox c = b;
  if (!strcmp(a.getLayerNameCStr(), m1_->getConstName())) {
    c.expand(m1Spacing_);
  } else {
    c.expand(cut1_->getSpacing());
  }
  return !c.intersects(a, false);
}
void ViaCandidateBuilder::getPinCandidate(
    std::vector<std::shared_ptr<ViaCandidate>>& ans,
    const ByBox& pin,
    const std::shared_ptr<ByViaInst>& via)
{
  ViaCandidate tmp(pin.getNetName(),
                   std::make_pair(pin.getInstName(), pin.getPinName()),
                   via);
  auto b = via->getBottomBox();
  BsPoint p(via->x(), via->y());
  if (pin.intersects(b)) {
    ans.emplace_back(std::make_shared<ViaCandidate>(tmp));
    return;
  }

  auto getLineBox = [&](int x1, int y1, int x2, int y2) {
    if (x1 > x2) {
      std::swap(x1, x2);
    }
    if (y1 > y2) {
      std::swap(y1, y2);
    }
    int w = m1_->getWidth() / 2;
    return ByBox(x1 - w,
                 y1 - w,
                 x2 + w,
                 y2 + w,
                 std::make_shared<ByBoxInfo>(m1_->getName(),
                                             pin.getNetName(),
                                             std::nullopt,
                                             std::nullopt,
                                             std::nullopt,
                                             ByBoxInfo::WIRE));
  };

  auto getPath = [&](const std::vector<BsPoint>& ps) {
    std::vector<ByBox> res;
    for (int i = 1; i < ps.size(); i++) {
      res.emplace_back(
          getLineBox(ps[i - 1].x(), ps[i - 1].y(), ps[i].x(), ps[i].y()));
    }
    tmp.setWires(res);
    ans.emplace_back(std::make_shared<ViaCandidate>(tmp));
    return;
  };

  if (p.y() > pin.yMax()) {
    if (p.x() < pin.xMin()) {
      BsPoint t(pin.xMin() + m1_->getWidth() / 2, pin.yMax());
      getPath({p, BsPoint(t.x(), p.y()), t});
    } else if (p.x() > pin.xMax()) {
      BsPoint t(pin.xMax() - m1_->getWidth() / 2, pin.yMax());
      getPath({p, BsPoint(t.x(), p.y()), t});
    } else {
      getPath({p, BsPoint(p.x(), pin.yMax())});
    }
  } else if (p.y() < pin.yMin()) {
    if (p.x() < pin.xMin()) {
      BsPoint t(pin.xMin() + m1_->getWidth() / 2, pin.yMin());
      getPath({p, BsPoint(t.x(), p.y()), t});
    } else if (p.x() > pin.xMax()) {
      BsPoint t(pin.xMax() - m1_->getWidth() / 2, pin.yMin());
      getPath({p, BsPoint(t.x(), p.y()), t});
    } else {
      getPath({p, BsPoint(p.x(), pin.yMin())});
    }
  } else if (p.x() < pin.xMin()) {
    if (p.y() < pin.yMin()) {
      BsPoint t(pin.xMin(), pin.yMin() + m1_->getWidth() / 2);
      getPath({p, BsPoint(p.x(), t.y()), t});
    } else if (p.y() > pin.yMax()) {
      BsPoint t(pin.xMin(), pin.yMax() - m1_->getWidth() / 2);
      getPath({p, BsPoint(p.x(), t.y()), t});
    } else {
      getPath({p, BsPoint(pin.xMin(), p.y())});
    }
  } else if (p.x() > pin.xMax()) {
    if (p.y() < pin.yMin()) {
      BsPoint t(pin.xMax(), pin.yMin() + m1_->getWidth() / 2);
      getPath({p, BsPoint(p.x(), t.y()), t});
    } else if (p.y() > pin.yMax()) {
      BsPoint t(pin.xMax(), pin.yMax() - m1_->getWidth() / 2);
      getPath({p, BsPoint(p.x(), t.y()), t});
    } else {
      getPath({p, BsPoint(pin.xMax(), p.y())});
    }
  }
  return;
}

}  // namespace banyan