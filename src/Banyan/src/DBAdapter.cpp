
#include "DBAdapter.hpp"

#include <assert.h>
#include <lemon/dijkstra.h>
#include <lemon/list_graph.h>

#include <cmath>
#include <utility>

#include "GroupedRange.hpp"
#include "db_sta/dbNetwork.hh"
#include "lrl/Laurel.hh"
#include "odb/ZException.h"
#include "odb/db.h"
#include "odb/dbShape.h"
#include "odb/dbTransform.h"
#include "odb/dbWireCodec.h"
#include "odb/geom.h"
#include "rsz/SteinerTree.hh"
#include "sta/Network.hh"
#include "stt/SteinerTreeBuilder.h"
#include "utl/Logger.h"

namespace banyan {

struct TrackLayerName
{
  typedef std::string value_type;
  value_type operator()(odb::dbTrackGrid* track) const
  {
    return track->getTechLayer()->getName();
  }
};

struct BoxLayerName
{
  typedef std::string value_type;
  value_type operator()(odb::dbBox* box) const
  {
    return box->getTechLayer()->getName();
  }
};

int OpenDBAdapter::calLayerPitch(odb::dbTechLayer* layer)
{
  return (int) (layer->getDirection() == odb::dbTechLayerDir::VERTICAL
                    ? layer->getPitchX()
                    : layer->getPitchY());
}

int OpenDBAdapter::defdist(int x)
{
  // odb::dbBlock* block = db_->getChip()->getBlock();
  // return x * (double) block->getDefUnits()
  //        / (double) block->getDbUnitsPerMicron();
  return x;
}

int64_t OpenDBAdapter::defArea(double area)
{
  odb::dbTech* tech = db_->getTech();
  int database_unit = tech->getLefUnits();
  int64_t defArea = std::roundl(area * database_unit * database_unit);
  return defArea;
}

void OpenDBAdapter::getDesignFromOpenDB()
{
  odb::dbBlock* block = db_->getChip()->getBlock();

  strs_.emplace_back(block->getName());
  design_.name = strs_.back().c_str();

  odb::Rect box;
  block->getDieArea(box);
  design_.xmin = defdist(box.xMin());
  design_.ymin = defdist(box.yMin());
  design_.xmax = defdist(box.xMax());
  design_.ymax = defdist(box.yMax());

  auto tech = db_->getTech();

  design_.manufacturing_grid = tech->getManufacturingGrid();
  design_.database_microns = tech->getDbUnitsPerMicron();

  design_.max_via_stack = maxViaStack_;
}

void OpenDBAdapter::getLayersFromOpenDB()
{
  auto tech = db_->getTech();

  for (auto layer : tech->getLayers()) {
    switch (layer->getType().getValue()) {
      case odb::dbTechLayerType::ROUTING:
        addRoutingLayer(layer);
        break;
      case odb::dbTechLayerType::CUT:
        addCutLayer(layer);
        break;
      case odb::dbTechLayerType::MASTERSLICE:
        addMasterSliceLayer(layer);
        break;
      case odb::dbTechLayerType::IMPLANT:
        addImplantLayer(layer);
        break;
      default:
        break;
    }
  }

  if (!minLayer_.empty()) {
    auto it = layerNameToIdxs_.find(minLayer_);
    if (it == layerNameToIdxs_.end()) {
      logger_->error(
          utl::BY, 780, "can not find minlayer for name {}", minLayer_);
    }
    if (it->second < belowRoutingLayerMaxIndex_ + 2) {
      logger_->error(
          utl::BY,
          781,
          "minlayer must be greater than 1, the current setting is [{},{}]",
          minLayer_,
          it->second - belowRoutingLayerMaxIndex_);
    }
    design_.min_layer = it->second;
  } else {
    design_.min_layer = belowRoutingLayerMaxIndex_ + 2;  // M2 by default
  }

  if (!maxLayer_.empty()) {
    auto it = layerNameToIdxs_.find(maxLayer_);
    if (it == layerNameToIdxs_.end()) {
      logger_->error(
          utl::BY, 782, "can not find maxlayer for name {}", maxLayer_);
    }
    if (it->second <= design_.min_layer) {
      logger_->error(utl::BY,
                     783,
                     "maxlayer must be greater than minlayer, the current "
                     "setting is min[{},{}] max[{},{}]",
                     minLayer_,
                     design_.min_layer - belowRoutingLayerMaxIndex_,
                     maxLayer_,
                     it->second - belowRoutingLayerMaxIndex_);
    }
    design_.max_layer = it->second;
  } else {
    design_.max_layer = layers_.size() - 1;
    if (design_.max_layer > belowRoutingLayerMaxIndex_ + 7) {
      design_.max_layer
          = belowRoutingLayerMaxIndex_ + 7;  // M6 at most by default
    }
  }
}

void OpenDBAdapter::addRoutingLayer(odb::dbTechLayer* layer)
{
  assert(layers_.size() > 0);
  assert(hasAddedCutLayer_);
  auto it = layerNameToIdxs_
                .insert(make_pair(layer->getName(), (int) layers_.size()))
                .first;

  auto minStep = layer->getMinStep();
  auto minStepMaxEdges = layer->getMinStepMaxEdges();
  auto minStepMaxLength = layer->getMinStepMaxLength();
  layers_.push_back({it->first.c_str(),
                     banyan_go::DIR_NONE,
                     (int) layer->getWidth(),
                     calLayerPitch(layer),
                     0,
                     0,
                     minStep,
                     minStepMaxEdges,
                     minStepMaxLength,
                     0,
                     0,
                     0,
                     0});
}

void OpenDBAdapter::addCutLayer(odb::dbTechLayer* layer)
{
  assert(layers_.size() > 0);
  layerNameToIdxs_.insert(make_pair(layer->getName(), -(int) layers_.size()));
  hasAddedCutLayer_ = true;
}

void OpenDBAdapter::addMasterSliceLayer(odb::dbTechLayer* layer)
{
  auto it = layerNameToIdxs_.insert(make_pair(layer->getName(), 0)).first;
  layers_.push_back({it->first.c_str(), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  belowRoutingLayerMaxIndex_++;
}

void OpenDBAdapter::addImplantLayer(odb::dbTechLayer* layer)
{
  auto it = layerNameToIdxs_.insert(make_pair(layer->getName(), 0)).first;
  layers_.push_back({it->first.c_str(), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  belowRoutingLayerMaxIndex_++;
}

void OpenDBAdapter::getTracksFromOpenDB()
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  const auto tracks = block->getTrackGrids();

  for (auto groups :
       utl::GroupBy(tracks.begin(), tracks.end(), TrackLayerName())) {
    int layerIdx = layerNameToIdxs_.find(groups.value)->second;
    if (layerIdx < 0) {
      logger_->info(utl::BY,
                    800,
                    "ignore tracks on layer[{}] {}",
                    layerIdx,
                    groups.value);
      continue;
    }

    size_t laTrackBegin = tracks_.size();
    std::vector<int> grid;
    int dir = banyan_go::DIR_NONE, tmin = 0, tmax = 0;
    for (auto track : groups.range) {
      if (track->getTechLayer()->getDirection()
          == odb::dbTechLayerDir::HORIZONTAL) {
        track->getGridY(grid);
        dir = banyan_go::DIR_HORIZONTAL;
        tmin = design_.xmin;
        tmax = design_.xmax;
        break;
      } else if (track->getTechLayer()->getDirection()
                 == odb::dbTechLayerDir::VERTICAL) {
        track->getGridX(grid);
        dir = banyan_go::DIR_VERTICAL;
        tmin = design_.ymin;
        tmax = design_.ymax;
        break;
      }
    }

    if (dir == banyan_go::DIR_NONE) {
      logger_->info(utl::BY,
                    805,
                    "ignore tracks on layer[{}] {} with dir NONE",
                    layerIdx,
                    groups.value);
    }

    for (auto line : grid) {
      tracks_.push_back({layerIdx, line, tmin, tmax});
    }

    layers_.at(layerIdx).dir = dir;
    layers_.at(layerIdx).track_begin = laTrackBegin;
    layers_.at(layerIdx).track_end = tracks_.size();
  }
}

void getLayerNameToTechViasMap(
    const odb::dbSet<odb::dbTechVia>& vias,
    std::map<std::string, std::vector<odb::dbTechVia*>>& layerNameToTechVias)
{
  for (auto via : vias) {
    std::string lname = via->getTopLayer()->getName();
    if (layerNameToTechVias.count(lname) == 0) {
      std::vector<odb::dbTechVia*> groupedVias;
      groupedVias.push_back(via);
      layerNameToTechVias[lname] = groupedVias;
    } else {
      layerNameToTechVias.find(lname)->second.push_back(via);
    }
  }
}

void getLayerNameToViasMap(
    const odb::dbSet<odb::dbVia>& vias,
    std::map<std::string, std::vector<odb::dbVia*>>& layerNameToDefVias)
{
  for (auto via : vias) {
    std::string lname = via->getTopLayer()->getName();
    if (layerNameToDefVias.count(lname) == 0) {
      std::vector<odb::dbVia*> groupedVias;
      groupedVias.push_back(via);
      layerNameToDefVias[lname] = groupedVias;
    } else {
      layerNameToDefVias.find(lname)->second.push_back(via);
    }
  }
}

void OpenDBAdapter::getViaDefsFromOpenDB()
{
  std::map<std::string, std::vector<odb::dbTechVia*>> layerNameToTechVias;

  if (auto tech = db_->getTech(); tech != nullptr) {
    const auto techVias = tech->getVias();
    getLayerNameToTechViasMap(techVias, layerNameToTechVias);
  }

  std::map<std::string, std::vector<odb::dbVia*>> layerNameToDefVias;
  if (auto chip = db_->getChip(); chip != nullptr) {
    if (auto block = chip->getBlock(); block != nullptr) {
      const auto vias = block->getVias();
      getLayerNameToViasMap(vias, layerNameToDefVias);
    }
  }

  for (const auto& iter : layerNameToIdxs_) {
    auto layerName = iter.first;
    auto layerIdx = iter.second;
    if (layerIdx < 0) {
      logger_->info(
          utl::BY, 800, "ignore viadefs on layer[{}] {}", layerIdx, layerName);
      continue;
    }

    size_t viaDefIndexBegin = viadefs_.size();
    for (auto via : layerNameToDefVias[layerName]) {
      addViaDef(via);
    }

    for (auto techVia : layerNameToTechVias[layerName]) {
      addViaDef(techVia);
    }

    layers_.at(layerIdx).viadef_begin = viaDefIndexBegin;
    layers_.at(layerIdx).viadef_end = viadefs_.size();
  }
}

void OpenDBAdapter::addViaDef(odb::dbObject* viaObj)
{
  std::string vname;
  int botLayerIdx;
  int topLayerIdx;
  odb::dbTechViaGenerateRule* rule;
  bool viaHasParams = false;
  odb::dbViaParams viaParams;
  odb::dbSet<odb::dbBox> boxes;

  switch (viaObj->getObjectType()) {
    case odb::dbViaObj: {
      auto via = static_cast<odb::dbVia*>(viaObj);
      vname = via->getName();
      botLayerIdx
          = AssertFind(layerNameToIdxs_, via->getBottomLayer()->getName());
      topLayerIdx = AssertFind(layerNameToIdxs_, via->getTopLayer()->getName());
      rule = via->getViaGenerateRule();
      viaHasParams = via->hasParams();
      if (viaHasParams && rule != nullptr) {
        via->getViaParams(viaParams);
      } else {
        boxes = via->getBoxes();
      }
      break;
    }
    case odb::dbTechViaObj: {
      auto techVia = static_cast<odb::dbTechVia*>(viaObj);
      vname = techVia->getName();
      botLayerIdx
          = AssertFind(layerNameToIdxs_, techVia->getBottomLayer()->getName());
      topLayerIdx
          = AssertFind(layerNameToIdxs_, techVia->getTopLayer()->getName());
      rule = techVia->getViaGenerateRule();
      viaHasParams = techVia->hasParams();
      if (viaHasParams && rule != nullptr) {
        techVia->getViaParams(viaParams);
      } else {
        boxes = techVia->getBoxes();
      }
      break;
    }
    default:
      assert(false);
  }

  if ((botLayerIdx < 0) || (topLayerIdx < 0)) {
    logger_->error(utl::BY,
                   801,
                   "invalid viadef {} layers {} {}",
                   vname,
                   topLayerIdx,
                   botLayerIdx);
    throw odb::ZException("invalid viadef layers");
  }
  assert(botLayerIdx != topLayerIdx);

  struct banyan_go::box botBox;
  struct banyan_go::box topBox;

  int viaBoxBegin = boxes_.size();

  int cutNums = 0;

  if (viaHasParams && (rule != nullptr)) {
    int xSize = defdist(viaParams.getXCutSize());
    int ySize = defdist(viaParams.getYCutSize());
    int xCutSpacing = defdist(viaParams.getXCutSpacing());
    int yCutSpacing = defdist(viaParams.getYCutSpacing());
    int xOrigin = defdist(viaParams.getXOrigin());
    int yOrigin = defdist(viaParams.getYOrigin());
    int xTopEnc = defdist(viaParams.getXTopEnclosure());
    int yTopEnc = defdist(viaParams.getYTopEnclosure());
    int xBotEnc = defdist(viaParams.getXBottomEnclosure());
    int yBotEnc = defdist(viaParams.getYBottomEnclosure());
    int xTopOffset = defdist(viaParams.getXTopOffset());
    int yTopOffset = defdist(viaParams.getYTopOffset());
    int xBotOffset = defdist(viaParams.getXBottomOffset());
    int yBotOffset = defdist(viaParams.getYBottomOffset());

    int currX = 0;
    int currY = 0;
    for (int i = 0; i < viaParams.getNumCutRows(); i++) {
      currX = 0;
      for (int j = 0; j < viaParams.getNumCutCols(); j++) {
        currX += xSize + xCutSpacing;
        cutNums++;
      }
      currY += ySize + yCutSpacing;
    }

    currX -= xCutSpacing;  // max cut X
    currY -= yCutSpacing;  // max cut Y

    int botTranformX = -currX / 2 + xOrigin + xBotOffset;
    int botTranformY = -currY / 2 + yOrigin + yBotOffset;
    botBox = {botLayerIdx,
              botTranformX - xBotEnc,
              botTranformY - yBotEnc,
              currX + xBotEnc * 2,
              currY + yBotEnc * 2};
    // currX + botTranformX + xBotEnc,
    // currY + botTranformY + yBotEnc};

    int topTranformX = -currX / 2 + xOrigin + xTopOffset;
    int topTranformY = -currY / 2 + yOrigin + yTopOffset;
    topBox = {topLayerIdx,
              topTranformX - xTopEnc,
              topTranformY - yTopEnc,
              currX + xTopEnc * 2,
              currY + yTopEnc * 2};
    // currX + topTranformX + xTopEnc,
    // currY + topTranformY + yTopEnc};
  } else {
    int cnt = 0;
    for (auto groups :
         utl::GroupBy(boxes.begin(), boxes.end(), BoxLayerName())) {
      for (auto box : groups.range) {
        switch (cnt) {
          case 0:
            botBox = {botLayerIdx,
                      defdist(box->xMin()),
                      defdist(box->yMin()),
                      defdist(box->getDX()),
                      defdist(box->getDY())};
            break;
          case 1:
            addBox(box);
            cutNums++;
            break;
          case 2:
            topBox = {topLayerIdx,
                      defdist(box->xMin()),
                      defdist(box->yMin()),
                      defdist(box->getDX()),
                      defdist(box->getDY())};

            break;
          default:
            break;
        }
      }
      cnt++;
    }
  }
  auto it
      = viaDefNameToIdxs_.insert(make_pair(vname, (int) viadefs_.size())).first;
  viadefs_.push_back({it->first.c_str(),
                      topBox,
                      botBox,
                      (banyan_go::GoUint64) viaBoxBegin,
                      (banyan_go::GoUint64) boxes_.size(),
                      cutNums});
}

void OpenDBAdapter::getNetsFromOpenDB()
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  for (auto net : block->getNets()) {
    addNet(net);
  }
}

std::map<const sta::Pin*, int> OpenDBAdapter::getDistFromSrcForSinks(
    sta::Pin* src,
    std::vector<sta::Pin*> sinks)
{
  std::map<const sta::Pin*, int> results;
  if (src == nullptr || sinks.size() <= 0) {
    return results;
  }
  auto network = laurel_->getDbNetwork();
  if (sinks.size() <= 0) {
    return results;
  }

  if (sinks.size() == 1) {
    odb::Point srcLoc = network->location(src);
    auto sink = sinks.at(0);
    odb::Point sinkLoc = network->location(sink);
    results[sink] = odb::Point::manhattanDistance(srcLoc, sinkLoc);
    return results;
  }

  typedef lemon::ListGraph Graph;
  typedef Graph::Node Node;
  typedef Graph::Edge Edge;
  typedef Graph::EdgeMap<int> LengthMap;

  Graph graph;
  std::map<int, Node> idx2node;
  LengthMap lenMap(graph);

  rsz::SteinerTree* tree
      = rsz::makeSteinerTree(src, true, 10000, sttBuilder_, network, logger_);

  int srcPt = tree->drvrPt(network);
  int branch_count = tree->branchCount();
  for (int branch = 0; branch < branch_count; branch++) {
    odb::Point parent;
    odb::Point child;
    int parentPt;
    int childPt;
    int wireLen;

    tree->branch(branch, parent, parentPt, child, childPt, wireLen);
    if (parentPt == childPt) {
      continue;
    }
    if (idx2node.find(parentPt) == idx2node.end()) {
      Node pn = graph.addNode();
      idx2node[parentPt] = pn;
    }
    if (idx2node.find(childPt) == idx2node.end()) {
      Node cn = graph.addNode();
      idx2node[childPt] = cn;
    }

    Edge e = graph.addEdge(idx2node[parentPt], idx2node[childPt]);
    lenMap.set(e, wireLen);
  }

  lemon::Dijkstra<Graph, LengthMap> dijkstra(graph, lenMap);
  dijkstra.run(idx2node[srcPt]);
  // tree->report(logger_, network);

  for (int branch = 0; branch < branch_count; branch++) {
    auto nodePt = branch;
    if (nodePt == srcPt) {
      continue;
    }
    const sta::PinSeq* pins = tree->pins(nodePt);

    if (pins) {
      for (int i = 0; i < pins->size(); i++) {
        const sta::Pin* pin = pins->at(i);
        results[pin] = dijkstra.dist(idx2node[nodePt]);
      }
    }
  }

  return results;
}

double OpenDBAdapter::getWorstHoldSlack()
{
  double worstHoldSlack = 0;
  odb::dbBlock* block = db_->getChip()->getBlock();

  for (auto net : block->getNets()) {
    auto bynetIndex = AssertFind(netNameToIdxs_, net->getName());
    auto bynet = nets_.at(bynetIndex);

    if (net->isSpecial() || bynet.pin_begin == bynet.pin_end - 1) {
      continue;
    }
    for (int i = bynet.pin_begin; i < bynet.pin_end; i++) {
      auto bypin = pins_.at(i);
      if (bypin.use == banyan_go::PIN_SOURCE) {
        auto src = getStaPin(bypin.name);
        worstHoldSlack
            = std::min(worstHoldSlack, (double) laurel_->getHoldSlack(src));
        break;
      }
    }
  }
  return worstHoldSlack;
}

sta::Pin* OpenDBAdapter::getStaPin(const char* name)
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  sta::dbNetwork* network = laurel_->getDbNetwork();
  auto iterm = block->findITerm(name);
  if (iterm != nullptr) {
    return network->dbToSta(iterm);
  }

  auto bterm = block->findBTerm(name);
  if (bterm != nullptr) {
    return network->dbToSta(bterm);
  }

  return nullptr;
}

void OpenDBAdapter::getWireBugdeting()
{
  double worstHoldSlack = 0;
  bool hasHoldVio = false;
  if (timingDriven_) {
    laurel_->wireBudgetingRC();
    worstHoldSlack = getWorstHoldSlack();
    if (worstHoldSlack < 0) {
      hasHoldVio = true;
    }
    logger_->info(utl::BY,
                  888,
                  "hasHoldVio {} worstHoldSlack {}",
                  hasHoldVio,
                  worstHoldSlack);
  }

  auto getHoldSlackRate = [=](sta::Pin* src) -> double {
    if (!hasHoldVio) {
      return 0;
    }
    auto holdSlack = laurel_->getHoldSlack(src);
    if (holdSlack >= 0) {
      return 0;
    }
    return holdSlack / worstHoldSlack * 100;
  };

  odb::dbBlock* block = db_->getChip()->getBlock();
  for (auto net : block->getNets()) {
    auto bynetIndex = AssertFind(netNameToIdxs_, net->getName());
    auto bynet = nets_.at(bynetIndex);

    if (net->isSpecial() || bynet.pin_begin == bynet.pin_end - 1) {
      continue;
    }

    sta::Pin* src = nullptr;
    int srcIdx = -1;
    std::vector<sta::Pin*> sinks;
    std::vector<int> sinkIdxes;

    for (int i = bynet.pin_begin; i < bynet.pin_end; i++) {
      auto bypin = pins_.at(i);
      auto staPin = getStaPin(bypin.name);
      if (bypin.use == banyan_go::PIN_SOURCE) {
        src = staPin;
        srcIdx = i;
      } else if (bypin.use == banyan_go::PIN_SINK) {
        sinks.push_back(staPin);
        sinkIdxes.push_back(i);
      }
    }

    if (src != nullptr && srcIdx != -1) {
      bynet.wire_budget_begin = tableTriples_.size();

      auto distmap = getDistFromSrcForSinks(src, sinks);
      auto holdSlackRate = getHoldSlackRate(src);
      for (size_t i = 0; i < sinks.size(); i++) {
        auto sink = sinks.at(i);
        int rate = timingDriven_
                       ? (int) (laurel_->getWireBudRate(src, sink) * 100)
                       : 0;
        if (rate >= 150 && holdSlackRate > 0) {
          rate += holdSlackRate;
        }
        // logger_->info(utl::BY,
        //               886,
        //               "src {} sink {} hold {} wirebudget {}",
        //               pins_[srcIdx].name,
        //               pins_[sinkIdxes[i]].name,
        //               holdSlackRate,
        //               rate);
        tableTriples_.push_back(
            {srcIdx, sinkIdxes[i], (banyan_go::GoUint64) rate});
        pins_.at(sinkIdxes[i]).min_dist_from_src = distmap[sink];
      }
      bynet.wire_budget_end = tableTriples_.size();
      nets_.at(bynetIndex) = bynet;
    }
  }
}

void OpenDBAdapter::addNet(odb::dbNet* net)
{
  int ndrIdx = -1;
  if (net->getNonDefaultRule() != nullptr) {
    ndrIdx = AssertFind(ndrNameToIdxs_, net->getNonDefaultRule()->getName());
  }
  auto it
      = netNameToIdxs_.insert(make_pair(net->getName(), nets_.size())).first;
  banyan_go::net_use use = banyan_go::USE_SIGNAL;
  switch (net->getSigType()) {
    case odb::dbSigType::SIGNAL:
      break;
    case odb::dbSigType::CLOCK:
      use = banyan_go::USE_CLOCK;
      break;
    case odb::dbSigType::POWER:
      use = banyan_go::USE_POWER;
      break;
    case odb::dbSigType::GROUND:
      use = banyan_go::USE_GROUND;
      break;
    default:
      logger_->error(utl::BY, 700, "unsupported NET USE {}", net->getSigType());
      throw odb::ZException("unsupported NET USE");
  }
  if (net->isSpecial()) {
    use = banyan_go::net_use(use + banyan_go::NET_SPECIAL);
  }
  nets_.push_back({it->first.c_str(), ndrIdx, use, 0, 0, 0, 0, 0, 0, 0, 0});
}

void OpenDBAdapter::addBox(odb::dbBox* box)
{
  int layerIdx = AssertFind(layerNameToIdxs_, box->getTechLayer()->getName());
  if (layerIdx < 0) {
    // logger_->info(utl::BY,
    //        802,
    //        "box on layer[{}] {}",
    //        layerIdx, box->getTechLayer()->getName());
  }

  boxes_.push_back({layerIdx,
                    defdist(box->xMin()),
                    defdist(box->yMin()),
                    defdist(box->getDX()),
                    defdist(box->getDY()),
                    banyan_go::DEFAULT_WIRE_EXT,
                    banyan_go::DEFAULT_WIRE_EXT});
}

void OpenDBAdapter::addBox(std::string layerName, const odb::Rect& rect)
{
  int layerIdx = AssertFind(layerNameToIdxs_, layerName);
  if (layerIdx < 0) {
    // logger_->info(utl::BY,
    //        802,
    //        "box on layer[{}] {}",
    //        layerIdx, layerName);
  }

  boxes_.push_back({layerIdx,
                    defdist(rect.xMin()),
                    defdist(rect.yMin()),
                    defdist(rect.dx()),
                    defdist(rect.dy()),
                    banyan_go::DEFAULT_WIRE_EXT,
                    banyan_go::DEFAULT_WIRE_EXT});
}

void OpenDBAdapter::addBox(std::string layerName,
                           int left,
                           int bottom,
                           int right,
                           int top)
{
  int layerIdx = AssertFind(layerNameToIdxs_, layerName);
  if (layerIdx < 0) {
    // logger_->info(utl::BY,
    //        802,
    //        "box on layer[{}] {}",
    //        layerIdx, layerName);
  }

  boxes_.push_back({layerIdx,
                    left,
                    bottom,
                    right - left,
                    top - bottom,
                    banyan_go::DEFAULT_WIRE_EXT,
                    banyan_go::DEFAULT_WIRE_EXT});
}

void OpenDBAdapter::addWireBox(const std::string& layerName,
                               int left,
                               int bottom,
                               int right,
                               int top,
                               int startExt,
                               int endExt)
{
  assert((startExt == banyan_go::DEFAULT_WIRE_EXT || startExt >= 0)
         && (endExt == banyan_go::DEFAULT_WIRE_EXT || endExt >= 0));
  int layerIdx = AssertFind(layerNameToIdxs_, layerName);
  if (layerIdx < 0) {
    // logger_->info(utl::BY,
    //        802,
    //        "box on layer[{}] {}",
    //        layerIdx, layerName);
  }
  boxes_.push_back(
      {layerIdx, left, bottom, right - left, top - bottom, startExt, endExt});
}

void OpenDBAdapter::addVia(odb::dbBox* box)
{
  auto getViaNameFromViaBox = [](odb::dbBox* box) -> std::string {
    assert(box->isVia());
    auto techVia = box->getTechVia();
    if (techVia != nullptr) {
      return techVia->getName();
    }
    auto blockVia = box->getBlockVia();
    if (blockVia != nullptr) {
      return blockVia->getName();
    }
    assert(false);
  };

  int x;
  int y;
  box->getViaXY(x, y);
  int viaDefIdx = AssertFind(viaDefNameToIdxs_, getViaNameFromViaBox(box));
  vias_.push_back({viaDefIdx, defdist(x), defdist(y)});
}

void OpenDBAdapter::addVia(std::string viaName, int x, int y)
{
  int viaDefIdx = AssertFind(viaDefNameToIdxs_, viaName);
  vias_.push_back({viaDefIdx, x, y});
}

void OpenDBAdapter::addSpecialPath(odb::dbSBox* box)
{
  auto layer = box->getTechLayer();
  int x1 = box->xMin();
  int y1 = box->yMin();
  int x2 = box->xMax();
  int y2 = box->yMax();
  uint dx = x2 - x1;
  uint dy = y2 - y1;
  // uint w;

  switch (box->getDirection()) {
    case odb::dbSBox::UNDEFINED: {
      bool dx_even = ((dx & 1) == 0);
      bool dy_even = ((dy & 1) == 0);

      if (dx_even && dy_even) {
        if (dy < dx) {
          // w = dy;
          uint dw = dy >> 1;
          y1 += dw;
          y2 -= dw;
          assert(y1 == y2);
        } else {
          // w = dx;
          uint dw = dx >> 1;
          x1 += dw;
          x2 -= dw;
          assert(x1 == x2);
        }
      } else if (dx_even) {
        // w = dx;
        uint dw = dx >> 1;
        x1 += dw;
        x2 -= dw;
        assert(x1 == x2);
      } else if (dy_even) {
        // w = dy;
        uint dw = dy >> 1;
        y1 += dw;
        y2 -= dw;
        assert(y1 == y2);
      } else {
        logger_->error(utl::BY, 702, "odd dimension in both directions");
        throw odb::ZException("odd dimension in both directions");
      }

      break;
    }

    case odb::dbSBox::HORIZONTAL: {
      // w = dy;
      // uint dw = dy >> 1;
      // y1 += dw;
      // y2 -= dw;
      // assert(y1 == y2);
      break;
    }

    case odb::dbSBox::VERTICAL: {
      // w = dx;
      // uint dw = dx >> 1;
      // x1 += dw;
      // x2 -= dw;
      // assert(x1 == x2);
      break;
    }

    case odb::dbSBox::OCTILINEAR: {
      logger_->error(utl::BY,
                     703,
                     "OCTILINEAR[{}] special path box not supported",
                     box->getDirection());
      throw odb::ZException("OCTILINEAR special path box not supported");

      odb::Oct oct = box->getOct();
      x1 = oct.getCenterLow().getX();
      y1 = oct.getCenterLow().getY();
      x2 = oct.getCenterHigh().getX();
      y2 = oct.getCenterHigh().getY();
      // w = oct.getWidth();
      break;
    }
    default:
      logger_->error(utl::BY, 704, "unknown direction");
      throw odb::ZException("unknown direction");
      break;
  }
  int layerIdx = layerNameToIdxs_.find(layer->getName())->second;
  if (layerIdx < 0) {
    logger_->info(utl::BY,
                  803,
                  "special path on layer[{}] {}",
                  layerIdx,
                  box->getTechLayer()->getName());
  }

  boxes_.push_back({layerIdx,
                    defdist(x1),
                    defdist(y1),
                    defdist(dx),
                    defdist(dy),
                    banyan_go::DEFAULT_WIRE_EXT,
                    banyan_go::DEFAULT_WIRE_EXT});
}

void OpenDBAdapter::addWire(odb::dbWire* wire)
{
  auto doAddWire = [&](const std::string& layerName,
                       int x1,
                       int y1,
                       int ext1,
                       int x2,
                       int y2,
                       int ext2) {
    int beginX = defdist(x1);
    int beginY = defdist(y1);
    int beginExt = defdist(ext1);
    int endX = defdist(x2);
    int endY = defdist(y2);
    int endExt = defdist(ext2);

    assert(beginX == endX || beginY == endY);
    if (beginX > endX || beginY > endY) {
      std::swap(beginX, endX);
      std::swap(beginY, endY);
      std::swap(beginExt, endExt);
    }
    addWireBox(layerName, beginX, beginY, endX, endY, beginExt, endExt);
  };

  auto doAddRect = [&](const std::string& layerName,
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
    addBox(layerName, defdist(x1), defdist(y1), defdist(x2), defdist(y2));
  };

  auto doAddVia = [&](const std::string& name, int x, int y) {
    addVia(name, defdist(x), defdist(y));
  };

  odb::dbWireDecoder decode;
  int prevX = std::numeric_limits<int>::max();
  int prevY = std::numeric_limits<int>::max();
  int prevExt = std::numeric_limits<int>::max();
  bool hasPrevCoor = false;

  for (decode.begin(wire);;) {
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
        ext = banyan_go::DEFAULT_WIRE_EXT;
        prevX = x;
        prevY = y;
        prevExt = ext;
        hasPrevCoor = true;
        break;
      }

      case odb::dbWireDecoder::POINT: {
        int x, y, ext;
        decode.getPoint(x, y);
        ext = banyan_go::DEFAULT_WIRE_EXT;

        if (hasPrevCoor) {
          doAddWire(
              decode.getLayer()->getName(), prevX, prevY, prevExt, x, y, ext);
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
          doAddWire(
              decode.getLayer()->getName(), prevX, prevY, prevExt, x, y, ext);
        }
        prevX = x;
        prevY = y;
        prevExt = ext;
        hasPrevCoor = true;
        break;
      }

      case odb::dbWireDecoder::VIA: {
        assert(hasPrevCoor);
        doAddVia(decode.getVia()->getName(), prevX, prevY);
        break;
      }

      case odb::dbWireDecoder::TECH_VIA: {
        assert(hasPrevCoor);
        doAddVia(decode.getTechVia()->getName(), prevX, prevY);
        break;
      }

      case odb::dbWireDecoder::RECT: {
        assert(hasPrevCoor);

        int deltaX1;
        int deltaY1;
        int deltaX2;
        int deltaY2;
        decode.getRect(deltaX1, deltaY1, deltaX2, deltaY2);
        doAddRect(decode.getLayer()->getName(),
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

      case odb::dbWireDecoder::END_DECODE:
        return;

      default:
        break;
    }
  }
}

void OpenDBAdapter::getPinsFromOpenDB()
{
  odb::dbBlock* block = db_->getChip()->getBlock();

  auto nets = block->getNets();
  for (odb::dbNet* net : nets) {
    size_t pinIndexBegin = pins_.size();

    const auto nName = net->getName();
    int netIdx = AssertFind(netNameToIdxs_, nName);

    for (auto bTerm : net->getBTerms()) {
      auto boxIndexBegin = boxes_.size();
      auto viaIndexBegin = vias_.size();

      for (auto bPin : bTerm->getBPins()) {
        for (auto box : bPin->getBoxes()) {
          // odb::dbTechLayer* techLayer = box->getTechLayer();
          // if (techLayer->getType().getValue()
          //     != odb::dbTechLayerType::ROUTING) {
          //   continue;
          // }
          assert(!box->isVia());
          addBox(box);
        }
      }
      banyan_go::pin_use use = bTerm->getIoType() == odb::dbIoType::OUTPUT
                                   ? banyan_go::PIN_SINK
                                   : banyan_go::PIN_SOURCE;
      strs_.emplace_back(bTerm->getName());
      pins_.push_back({strs_.back().c_str(),
                       use,
                       0,
                       boxIndexBegin,
                       boxes_.size(),
                       viaIndexBegin,
                       vias_.size()});
    }
    for (auto iTerm : net->getITerms()) {
      auto boxIndexBegin = boxes_.size();
      auto viaIndexBegin = vias_.size();

      auto inst = iTerm->getInst();
      auto mTerm = iTerm->getMTerm();

      int pX, pY;
      inst->getOrigin(pX, pY);
      odb::Point origin = odb::Point(pX, pY);
      odb::dbTransform transform(inst->getOrient(), origin);

      for (auto mPin : mTerm->getMPins()) {
        for (auto box : mPin->getGeometry()) {
          if (box->isVia()) {
            addVia(box);
          } else {
            odb::dbTechLayer* techLayer = box->getTechLayer();
            odb::Rect rect;
            box->getBox(rect);
            transform.apply(rect);
            addBox(techLayer->getName(), rect);
          }
        }
      }

      banyan_go::pin_use use = iTerm->getIoType() == odb::dbIoType::INPUT
                                   ? banyan_go::PIN_SINK
                                   : banyan_go::PIN_SOURCE;
      strs_.emplace_back(inst->getName() + "/" + mTerm->getName());
      pins_.push_back({strs_.back().c_str(),
                       use,
                       0,
                       boxIndexBegin,
                       boxes_.size(),
                       viaIndexBegin,
                       vias_.size()});
      instPinNameToIdxs_[strs_.back()] = pins_.size() - 1;
    }

    if (!net->isSpecial() && net->getWire() != nullptr) {
      auto boxIndexBegin = boxes_.size();
      auto viaIndexBegin = vias_.size();
      addWire(net->getWire());

      strs_.emplace_back(nName + "/wire");
      pins_.push_back({strs_.back().c_str(),
                       banyan_go::PIN_WIRE,
                       0,
                       boxIndexBegin,
                       boxes_.size(),
                       viaIndexBegin,
                       vias_.size()});
    }

    if (net->isSpecial()) {
      auto boxIndexBegin = boxes_.size();
      auto viaIndexBegin = vias_.size();
      for (auto swire : net->getSWires()) {
        for (auto box : swire->getWires()) {
          if (!box->isVia()) {
            addSpecialPath(box);
          } else {
            addVia(box);
          }
        }
      }
      if ((boxIndexBegin != boxes_.size()) || (viaIndexBegin != vias_.size())) {
        strs_.emplace_back(nName + "/specialwire");
        pins_.push_back({strs_.back().c_str(),
                         banyan_go::PIN_WIRE,
                         0,
                         boxIndexBegin,
                         boxes_.size(),
                         viaIndexBegin,
                         vias_.size()});
      }
    }

    nets_.at(netIdx).pin_begin = pinIndexBegin;
    nets_.at(netIdx).pin_end = pins_.size();
  }
}

void OpenDBAdapter::getNDRsFromOpenDB()
{
  for (auto ndr : db_->getTech()->getNonDefaultRules()) {
    addNDR(ndr);
  }
  for (auto ndr : db_->getChip()->getBlock()->getNonDefaultRules()) {
    addNDR(ndr);
  }
}

void OpenDBAdapter::addNDR(odb::dbTechNonDefaultRule* ndr)
{
  std::vector<odb::dbTechLayerRule*> laNDRs;
  ndr->getLayerRules(laNDRs);
  auto ndrViaIdxBegin = vias_.size();
  auto ndrLayerIdxBegin = ndrlayers_.size();
  for (auto& laNDR : laNDRs) {
    auto lname = laNDR->getLayer()->getName();
    auto layerIdx = AssertFind(layerNameToIdxs_, lname);
    ndrlayers_.push_back({layerIdx,
                          laNDR->getWidth(),
                          laNDR->getSpacing(),
                          laNDR->getWireExtension()});
  }
  std::vector<odb::dbTechVia*> vias;
  ndr->getVias(vias);
  for (auto via : vias) {
    vias_.push_back({(int) viadefs_.size()});
    addViaDef(via);
  }

  std::vector<odb::dbTechVia*> useVias;
  ndr->getUseVias(useVias);
  for (auto useVia : useVias) {
    auto viadefIdx = AssertFind(viaDefNameToIdxs_, useVia->getName());
    vias_.push_back({(int) viadefIdx});
  }

  std::vector<odb::dbTechViaGenerateRule*> useViaRules;
  ndr->getUseViaRules(useViaRules);
  for (auto viaRule : useViaRules) {
    auto viadefIdx = AssertFind(viaDefNameToIdxs_, viaRule->getName());
    vias_.push_back({(int) viadefIdx});
  }
  ndrNameToIdxs_[ndr->getName()] = ndrs_.size();
  ndrs_.push_back(banyan_go::ndr{ndr->getName().c_str(),
                                 ndr->getHardSpacing(),
                                 ndrViaIdxBegin,
                                 vias.size(),
                                 ndrLayerIdxBegin,
                                 ndrlayers_.size()});
}

void OpenDBAdapter::getObsFromOpenDB()
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  auto rtBlockageIds = block->getDieAreaRoutingBlockageIds();
  std::set<uint> rtBlockageIdSet(rtBlockageIds.begin(), rtBlockageIds.end());

  for (auto blockage : block->getObstructions()) {
    size_t boxIndexBegin = boxes_.size();
    size_t viaIndexBegin = vias_.size();
    auto layerName = blockage->getBBox()->getTechLayer()->getName();
    // int layerIdx = AssertFind(layerNameToIdxs_, layerName);
    // if (layerIdx < obs_layers_) {
    //   continue;
    // }
    int xl = blockage->getBBox()->xMin();
    int yl = blockage->getBBox()->yMin();
    int xh = blockage->getBBox()->xMax();
    int yh = blockage->getBBox()->yMax();
    addBox(layerName, xl, yl, xh, yh);
    int use = banyan_go::PIN_OBS;
    if (rtBlockageIdSet.find(blockage->getId()) != rtBlockageIdSet.end()) {
      use = banyan_go::PIN_DIEAREA_OBS;
      // logger_->info(
      // utl::BY, 705, "diearea blockage {} [{},{},{},{}]", layerName, xl, yl,
      // xh, yh);
    }
    pins_.push_back({"obs",
                     use,
                     0,
                     boxIndexBegin,
                     boxes_.size(),
                     viaIndexBegin,
                     vias_.size()});
  }
}

void OpenDBAdapter::getInstsObsFromOpenDB()
{
  std::map<std::string, std::vector<std::pair<std::string, odb::Rect>>>
      mnameToBoxes;
  for (auto lib : db_->getLibs()) {
    for (odb::dbMaster* master : lib->getMasters()) {
      // for (auto term : master->getMTerms()) {
      //   for (auto mpin : term->getMPins()) {
      //     for (auto box : mpin->getGeometry()) {

      //     }
      //   }
      // }
      std::vector<std::pair<std::string, odb::Rect>> boxes;
      for (auto obs : master->getObstructions()) {
        std::string layerName = obs->getTechLayer()->getName();
        // int layerIdx = AssertFind(layerNameToIdxs_, layerName);
        // if (layerIdx < obs_layers_) {
        //   continue;
        // }
        odb::Rect rect(obs->xMin(), obs->yMin(), obs->xMax(), obs->yMax());
        boxes.push_back(std::make_pair(layerName, rect));
      }
      if (boxes.size() > 0) {
        mnameToBoxes[master->getName()] = boxes;
      }
    }
  }

  auto block = db_->getChip()->getBlock();
  for (auto inst : block->getInsts()) {
    int pX, pY;
    inst->getOrigin(pX, pY);
    odb::Point origin = odb::Point(pX, pY);
    odb::dbTransform transform(inst->getOrient(), origin);
    size_t boxIndexBegin = boxes_.size();
    size_t viaIndexBegin = vias_.size();

    for (auto iTerm : inst->getITerms()) {
      auto mTerm = iTerm->getMTerm();
      auto pinNameInBanyan = inst->getName() + "/" + mTerm->getName();
      auto iter = instPinNameToIdxs_.find(pinNameInBanyan);
      if (iter == instPinNameToIdxs_.end()) {
        for (auto mPin : mTerm->getMPins()) {
          for (auto box : mPin->getGeometry()) {
            odb::dbTechLayer* techLayer = box->getTechLayer();
            // if (techLayer->getType().getValue()
            //     != odb::dbTechLayerType::ROUTING) {
            //   continue;
            // }

            odb::Rect rect;
            box->getBox(rect);
            transform.apply(rect);
            assert(!box->isVia());
            addBox(techLayer->getName(), rect);
          }
        }
      }
    }
    if (boxes_.size() > boxIndexBegin) {
      strs_.emplace_back(inst->getName() + "/inst_unused");
      pins_.push_back({strs_.back().c_str(),
                       banyan_go::PIN_INST_UNUSED,
                       0,
                       boxIndexBegin,
                       boxes_.size(),
                       viaIndexBegin,
                       vias_.size()});
      boxIndexBegin = boxes_.size();
      viaIndexBegin = vias_.size();
    }

    auto iter = mnameToBoxes.find(inst->getMaster()->getName());
    if (iter != mnameToBoxes.end()) {
      for (auto box : iter->second) {
        auto instObsRect = box.second;
        transform.apply(instObsRect);
        addBox(box.first, instObsRect);
      }
    }
    if (boxes_.size() > boxIndexBegin) {
      strs_.emplace_back(inst->getName() + "/inst_obs");
      pins_.push_back({strs_.back().c_str(),
                       banyan_go::PIN_INST_OBS,
                       0,
                       boxIndexBegin,
                       boxes_.size(),
                       viaIndexBegin,
                       vias_.size()});
    }
  }
}

void OpenDBAdapter::updateDB(std::vector<banyan_go::layer>& layers,
                             std::vector<banyan_go::net>& nets,
                             std::vector<banyan_go::viadef>& viadefs,
                             banyan_go::route_ret& ret)
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  odb::dbTech* tech = db_->getTech();

  odb::dbWireEncoder _wire_encoder;
  for (auto net : nets) {
    auto netInOpenDB = block->findNet(net.name);
    if (netInOpenDB == nullptr) {
      continue;
    }

    auto wire = odb::dbWire::create(netInOpenDB);
    if (wire == nullptr) {
      wire = netInOpenDB->getWire();
    }
    _wire_encoder.begin(wire);

    // add wire box
    for (size_t i = net.rbox_begin; i < net.rbox_end; i++) {
      auto box = ret.rboxes[i];
      auto layer = layers[box.la];
      auto layerInOpenDB = tech->findLayer(layer.name);
      // int width = layerInOpenDB->getWidth();
      // TODO: pathSeg->isTapered()
      if (!netInOpenDB->getNonDefaultRule()) {
        _wire_encoder.newPath(layerInOpenDB, odb::dbWireType("ROUTED"));
      } else {
        auto ndrLayerRule
            = netInOpenDB->getNonDefaultRule()->getLayerRule(layerInOpenDB);
        // width = ndrLayerRule->getWidth();
        _wire_encoder.newPath(
            layerInOpenDB, odb::dbWireType("ROUTED"), ndrLayerRule);
      }
      if (box.w == 0) {
        _wire_encoder.addPoint(box.x, box.y);
        _wire_encoder.addPoint(box.x, box.y + box.h);
      } else if (box.h == 0) {
        _wire_encoder.addPoint(box.x, box.y);
        _wire_encoder.addPoint(box.x + box.w, box.y);
      } else {
        _wire_encoder.addPoint(box.x, box.y);
        _wire_encoder.addRect(0, 0, box.w, box.h);
      }
    }

    // add wire via
    for (size_t i = net.rvia_begin; i < net.rvia_end; i++) {
      auto via = ret.rvias[i];
      if (via.viadef_index < 0) {
        logger_->error(
            utl::BY, 705, "invalid viadef index[{}]", via.viadef_index);
        throw odb::ZException("invalid viadef index");
      }
      auto viadef = viadefs[via.viadef_index];
      auto viaName = viadef.name;
      auto layer = layers[viadef.bottom.la];
      auto layerInOpenDB = tech->findLayer(layer.name);

      // TODO: via->isTapered()
      if (!netInOpenDB->getNonDefaultRule()) {
        _wire_encoder.newPath(layerInOpenDB, odb::dbWireType("ROUTED"));
      } else {
        _wire_encoder.newPath(
            layerInOpenDB,
            odb::dbWireType("ROUTED"),
            netInOpenDB->getNonDefaultRule()->getLayerRule(layerInOpenDB));
      }
      _wire_encoder.addPoint(via.x, via.y);

      odb::dbTechVia* tech_via = tech->findVia(viaName);
      if (tech_via != nullptr) {
        _wire_encoder.addTechVia(tech_via);
      } else {
        odb::dbVia* db_via = block->findVia(viaName);
        _wire_encoder.addVia(db_via);
      }
    }

    _wire_encoder.end();
  }
}

void OpenDBAdapter::printDB()
{
  printer.print("BanyanDB");
  printer.print("\nDesign:");
  printer.print("\tName: ", design_.name);
  printer.print("\tCoreRegion: ",
                design_.xmin,
                " ",
                design_.ymin,
                " ",
                design_.xmax,
                " ",
                design_.ymax);
  printer.print("\tManufacturingGrid: ", design_.manufacturing_grid);
  printer.print("\tDatabaseMicrons: ", design_.database_microns);
  printer.print("\tMaxViaStack: ", design_.max_via_stack);
  printer.print("\tMinLayer: ", design_.min_layer);
  printer.print("\tMaxLayer: ", design_.max_layer);
}

}  // namespace banyan
