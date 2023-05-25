#include "PinCluster.hpp"

#include "thread_pool/ThreadPool.h"

namespace banyan {

std::vector<std::vector<PinCluster>> banyan::PinClusterBuilder::getPinClusters()
{
  auto tech = db_->getTech();
  auto fl = ByAdapter::findFirstRoutingLayer(tech);

  DebugDefWriter dw(bydb_->getCoreArea().xMax(), bydb_->getCoreArea().yMax());

  auto checkBox = [&](const ByBox& b) {
    return b.getLayerName() == fl->getName()
           && bydb_->getCoreArea().intersects(b);
  };

  std::vector<std::shared_ptr<ByBox>> flBoxes;
  for (const auto& b : bydb_->getLayerBoxes({fl->getName()})) {
    flBoxes.emplace_back(std::make_shared<ByBox>(*b));
  }

  int maxPower = 0;
  std::vector<std::shared_ptr<ByBox>> pows;
  for (auto net : bydb_->getNets()) {
    if (net->isSpecial()) {
      for (auto b : net->getWires()) {
        if (b->getType() == ByBoxInfo::SPECIAL_WIRE && checkBox(*b)) {
          pows.emplace_back(std::make_shared<ByBox>(*b));
          maxPower = std::max(maxPower, b->yMax() - b->yMin());
        }
      }
    }
  }

  auto rows = bydb_->getRows();
  for (auto& b : rows) {
    b = std::make_shared<ByBox>(*b);
    b->narrowNS(maxPower / 2);
  }
  time_t rbt = time(nullptr);
  std::vector<std::vector<std::shared_ptr<ByBox>>> rowBoxes;
  ByDb::getRowBoxes(rowBoxes, rows, flBoxes);
  logger_->info(utl::BY, 1, "rowBoxTime:{}s", time(nullptr) - rbt);
  logger_->info(utl::BY,
                1,
                "pows:{}  rows:{} dbrow:{} flb:{}",
                pows.size(),
                rows.size(),
                db_->getChip()->getBlock()->getRows().size(),
                flBoxes.size());

  int spacing = std::max(fl->getSpacing(), ByAdapter::getEndOfLineSpacing(fl))
                + fl->getWidth() / 2;
  for (auto& b : flBoxes) {
    b->expand(spacing - 1);
  }

  time_t pcst = time(nullptr);
  std::vector<std::vector<PinCluster>> pcs;
  int enableCPUNum = sysconf(_SC_NPROCESSORS_ONLN);
  ThreadPool pool((enableCPUNum - 1) * 2);
  std::vector<std::future<std::vector<PinCluster>>> futurePinClusters;
  for (int i = 0; i < rows.size(); i++) {
    rows[i]->narrowNS(spacing);
    futurePinClusters.emplace_back(pool.enqueue(
        [this](const std::shared_ptr<ByBox>& row,
               const std::vector<std::shared_ptr<ByBox>>& boxes) {
          return this->getPinClusters(row, boxes);
        },
        std::ref(rows[i]),
        std::ref(rowBoxes[i])));
  }

  int pcCnt = 0;
  for (auto& f : futurePinClusters) {
    auto res = f.get();
    for (auto& pc : res) {
      pcCnt++;
      for (auto& b : pc.boxes()) {
        b.narrow(spacing - 1);
      }
    }
    pcs.emplace_back(res);
  }
  logger_->info(utl::BY, 1, "pc:{}  time:{}", pcCnt, time(nullptr) - pcst);

  return pcs;
}
std::vector<PinCluster> PinClusterBuilder::getPinClusters(
    const std::shared_ptr<ByBox>& row,
    const std::vector<std::shared_ptr<ByBox>>& boxes)
{
  using Value = std::pair<ByBox, int>;
  std::vector<PinCluster> ans;
  std::unordered_map<std::string, std::vector<int>> instMap;
  std::vector<Value> rtVal;
  rtVal.reserve(boxes.size());
  for (int i = 0; i < boxes.size(); i++) {
    auto iName = boxes[i]->getInstName();
    if (iName != "") {
      instMap[iName].emplace_back(i);
    }
    rtVal.emplace_back(*boxes[i], i);
  }
  bgi::rtree<Value, bgi::rstar<32>> rt(rtVal.begin(), rtVal.end());
  auto solve = [&](const ByBox& bbox) {
    std::vector<Value> res;
    std::vector<ByBox> ans;
    rt.query(bgi::intersects(bbox), std::back_inserter(res));
    for (const auto& v : res) {
      ans.emplace_back(*boxes[v.second]);
    }
    return ans;
  };
  std::vector<std::pair<int, int>> v;

  for (auto& [iName, bids] : instMap) {
    int x1 = std::numeric_limits<int>::max();
    int x2 = std::numeric_limits<int>::min();
    for (auto& id : bids) {
      const auto& b = boxes[id];
      x1 = std::min(x1, b->xMin());
      x2 = std::max(x2, b->xMax());
    }
    v.emplace_back(std::make_pair(x1, x2));
  }
  std::sort(v.begin(), v.end());
  int len = v.size();
  for (int i = 0; i < len - 2; i += 1) {
    int x1 = std::min(v[i].first, v[i + 2].first);
    int x2 = std::max(v[i].second, v[i + 2].second);
    ans.emplace_back(ByBox(x1, row->yMin(), x2, row->yMax()),
                     solve(ByBox(x1, row->yMin(), x2, row->yMax())));
  }
  std::sort(
      ans.begin(), ans.end(), [](const PinCluster& lhs, const PinCluster& rhs) {
        if (lhs.bbox().xMax() == rhs.bbox().xMax()) {
          return lhs.bbox().xMin() < rhs.bbox().xMin();
        }
        return lhs.bbox().xMax() < rhs.bbox().xMax();
      });
  return ans;
}

}  // namespace banyan
