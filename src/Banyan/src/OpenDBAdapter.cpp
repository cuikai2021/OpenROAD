
#include <assert.h>

#include <utility>

#include "DBAdapter.h"
#include "GroupedRange.hpp"
#include "opendb/ZException.h"
#include "opendb/db.h"
#include "opendb/dbTransform.h"
#include "opendb/dbWireCodec.h"
#include "opendb/geom.h"
#include "utl/Logger.h"

namespace banyan {

char* copy_string(std::string in_str)
{
  char* out_str = new char[in_str.length() + 1];
  in_str.copy(out_str, in_str.length());
  out_str[in_str.length()] = '\0';
  return out_str;
}

std::string getViaNameFromViaBox(odb::dbSBox* box)
{
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
}

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

struct ViaTopLayer
{
  typedef std::string value_type;
  value_type operator()(odb::dbVia* via) const
  {
    return via->getTopLayer()->getName();
  }
};

int OpenDBAdapter::calLayerPitch(odb::dbTechLayer* layer)
{
  return (int) (layer->getPitchX() > 0 ? layer->getPitchX()
                                       : layer->getPitchY());
}

int OpenDBAdapter::defdist(int x)
{
  odb::dbBlock* block = db_->getChip()->getBlock();

  return x * (double) block->getDefUnits()
         / (double) block->getDbUnitsPerMicron();
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
      default:
        break;
    }
  }
}

void OpenDBAdapter::addRoutingLayer(odb::dbTechLayer* layer)
{
  assert(layers_.size() > 0);
  assert(hasAddedMasterSliceLayer_ && hasAddedCutLayer_);
  layerNameToIdxs_.insert(make_pair(layer->getName(), (int) layers_.size()));
  if (layer->getPitchX() != 0) {
    layers_.push_back({copy_string(layer->getName()),
                       (int) layer->getWidth(),
                       calLayerPitch(layer),
                       0,
                       0,
                       0,
                       0});
  } else {
    layers_.push_back({copy_string(layer->getName()),
                       (int) layer->getWidth(),
                       calLayerPitch(layer),
                       0,
                       0,
                       0,
                       0});
  }
}

void OpenDBAdapter::addCutLayer(odb::dbTechLayer* layer)
{
  assert(layers_.size() > 0);
  assert(hasAddedMasterSliceLayer_);

  layerNameToIdxs_.insert(make_pair(layer->getName(), -(int) layers_.size()));
  // layers_.push_back({copy_string(layer->getName()),
  //    (int) layer->getWidth(), calLayerPitch(layer), 0, 0, 0, 0});
  hasAddedCutLayer_ = true;
}

void OpenDBAdapter::addMasterSliceLayer(odb::dbTechLayer* layer)
{
  assert(layers_.empty());
  assert(!hasAddedMasterSliceLayer_ && !hasAddedCutLayer_);

  if (layer->getLef58Type() != odb::dbTechLayer::LEF58_TYPE::NWELL
      && layer->getLef58Type() != odb::dbTechLayer::LEF58_TYPE::PWELL
      && layer->getLef58Type() != odb::dbTechLayer::LEF58_TYPE::DIFFUSION) {
    layerNameToIdxs_.insert(make_pair(layer->getName(), 0));
    layers_.push_back({copy_string(layer->getName()), 0, 0, 0, 0, 0, 0});
    hasAddedMasterSliceLayer_ = true;
  }
}

void OpenDBAdapter::getTracksFromOpenDB()
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  const auto tracks = block->getTrackGrids();
  size_t trackIndex = 0;
  for (auto group :
       utl::group_by(tracks.begin(), tracks.end(), TrackLayerName())) {
    int layerIdx = layerNameToIdxs_.find(group.value)->second;
    if (layerIdx < 0) {
      logger_->info(utl::BY,
                    800,
                    "ignore viadefs on layer[{}] {}",
                    layerIdx,
                    group.value);
      continue;
    }

    size_t laTrackBegin = trackIndex;
    for (auto track : group.range) {
      std::vector<int> gridX;
      std::vector<int> gridY;
      track->getGridX(gridX);
      track->getGridY(gridY);
      tracks_.push_back({layerIdx,
                         (int) gridX.size(),
                         (int) gridX.front(),
                         (int) gridX.back()});
      trackIndex++;
      tracks_.push_back({layerIdx,
                         (int) gridY.size(),
                         (int) gridY.front(),
                         (int) gridY.back()});
      trackIndex++;
    }
    layers_.at(layerIdx).track_begin = laTrackBegin;
    layers_.at(layerIdx).track_end = trackIndex;
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
  odb::dbTech* tech = db_->getTech();
  const auto techVias = tech->getVias();
  std::map<std::string, std::vector<odb::dbTechVia*>> layerNameToTechVias;
  getLayerNameToTechViasMap(techVias, layerNameToTechVias);

  odb::dbBlock* block = db_->getChip()->getBlock();
  const auto vias = block->getVias();
  std::map<std::string, std::vector<odb::dbVia*>> layerNameToDefVias;
  getLayerNameToViasMap(vias, layerNameToDefVias);

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
  bool viaHasParams;
  odb::dbViaParams viaParams;
  odb::dbSet<odb::dbBox> boxes;

  switch (viaObj->getObjectType()) {
    case odb::dbViaObj: {
      auto via = static_cast<odb::dbVia*>(viaObj);
      vname = via->getName();
      botLayerIdx
          = layerNameToIdxs_.find(via->getBottomLayer()->getName())->second;
      topLayerIdx
          = layerNameToIdxs_.find(via->getTopLayer()->getName())->second;
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
          = layerNameToIdxs_.find(techVia->getBottomLayer()->getName())->second;
      topLayerIdx
          = layerNameToIdxs_.find(techVia->getTopLayer()->getName())->second;
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
              currX + botTranformX + xBotEnc,
              currY + botTranformY + yBotEnc};

    int topTranformX = -currX / 2 + xOrigin + xTopOffset;
    int topTranformY = -currY / 2 + yOrigin + yTopOffset;
    topBox = {topLayerIdx,
              topTranformX - xTopEnc,
              topTranformY - yTopEnc,
              currX + topTranformX + xTopEnc,
              currY + topTranformY + yTopEnc};
  } else {
    int cnt = 0;
    for (auto group :
         utl::group_by(boxes.begin(), boxes.end(), BoxLayerName())) {
      for (auto box : group.range) {
        switch (cnt) {
          case 0:
            botBox = {botLayerIdx,
                      defdist(box->xMin()),
                      defdist(box->yMin()),
                      defdist(box->getDX()),
                      defdist(box->getDY())};
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
  viaDefNameToIdxs_.insert(make_pair(vname, (int) viadefs_.size()));
  viadefs_.push_back({copy_string(vname), botBox, topBox});
}

void OpenDBAdapter::getNetsFromOpenDB()
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  for (auto net : block->getNets()) {
    addNet(net);
  }
}

void OpenDBAdapter::addNet(odb::dbNet* net)
{
  netNameToIdxs_.insert(make_pair(net->getName(), nets_.size()));
  nets_.push_back({copy_string(net->getName()), 0, 0, 0, 0, 0, 0});
}

void OpenDBAdapter::addBox(odb::dbBox* box)
{
  int layerIdx = layerNameToIdxs_.find(box->getTechLayer()->getName())->second;
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
                    defdist(box->getDY())});
}

void OpenDBAdapter::addBox(std::string layerName, const odb::Rect& rect)
{
  int layerIdx = layerNameToIdxs_.find(layerName)->second;
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
                    defdist(rect.dy())});
}

void OpenDBAdapter::addBox(std::string layerName,
                           int left,
                           int bottom,
                           int right,
                           int top)
{
  int layerIdx = layerNameToIdxs_.find(layerName)->second;
  if (layerIdx < 0) {
    // logger_->info(utl::BY,
    //        802,
    //        "box on layer[{}] {}",
    //        layerIdx, layerName);
  }

  boxes_.push_back({layerIdx, left, bottom, right - left, top - bottom});
}

void OpenDBAdapter::addVia(odb::dbSBox* box)
{
  assert(viaDefNameToIdxs_.find(getViaNameFromViaBox(box))
         != viaDefNameToIdxs_.end());
  int x;
  int y;
  box->getViaXY(x, y);
  int viaDefIdx = viaDefNameToIdxs_.find(getViaNameFromViaBox(box))->second;
  vias_.push_back({viaDefIdx, defdist(x), defdist(y)});
}

void OpenDBAdapter::addVia(std::string viaName, int x, int y)
{
  assert(viaDefNameToIdxs_.find(viaName) != viaDefNameToIdxs_.end());
  int viaDefIdx = viaDefNameToIdxs_.find(viaName)->second;
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
  uint w;

  switch (box->getDirection()) {
    case odb::dbSBox::UNDEFINED: {
      bool dx_even = ((dx & 1) == 0);
      bool dy_even = ((dy & 1) == 0);

      if (dx_even && dy_even) {
        if (dy < dx) {
          w = dy;
          uint dw = dy >> 1;
          y1 += dw;
          y2 -= dw;
          assert(y1 == y2);
        } else {
          w = dx;
          uint dw = dx >> 1;
          x1 += dw;
          x2 -= dw;
          assert(x1 == x2);
        }
      } else if (dx_even) {
        w = dx;
        uint dw = dx >> 1;
        x1 += dw;
        x2 -= dw;
        assert(x1 == x2);
      } else if (dy_even) {
        w = dy;
        uint dw = dy >> 1;
        y1 += dw;
        y2 -= dw;
        assert(y1 == y2);
      } else {
        throw odb::ZException("odd dimension in both directions");
      }

      break;
    }

    case odb::dbSBox::HORIZONTAL: {
      w = dy;
      uint dw = dy >> 1;
      y1 += dw;
      y2 -= dw;
      assert(y1 == y2);
      break;
    }

    case odb::dbSBox::VERTICAL: {
      w = dx;
      uint dw = dx >> 1;
      x1 += dw;
      x2 -= dw;
      assert(x1 == x2);
      break;
    }
    case odb::dbSBox::OCTILINEAR: {
      odb::Oct oct = box->getOct();
      x1 = oct.getCenterLow().getX();
      y1 = oct.getCenterLow().getY();
      x2 = oct.getCenterHigh().getX();
      y2 = oct.getCenterHigh().getY();
      w = oct.getWidth();
      break;
    }
    default:
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

  boxes_.push_back(
      {layerIdx, defdist(x1), defdist(y1), defdist(dx), defdist(dy)});
}

void OpenDBAdapter::addWire(odb::dbWire* wire)
{
  std::string layerName = "";
  std::string viaName = "";
  std::string shape = "";
  bool hasBeginPoint = false;
  bool hasEndPoint = false;
  int beginX = -1;
  int beginY = -1;
  int beginExt = -1;
  int endX = -1;
  int endY = -1;
  int endExt = -1;
  bool hasRect = false;
  int left = -1;
  int bottom = -1;
  int right = -1;
  int top = -1;
  int width = 0;
  odb::dbWireDecoder decoder;
  decoder.begin(wire);
  odb::dbWireDecoder::OpCode pathId = decoder.next();
  while (pathId != odb::dbWireDecoder::END_DECODE) {
    layerName = "";
    viaName = "";
    shape = "";
    hasBeginPoint = false;
    hasEndPoint = false;
    beginX = -1;
    beginY = -1;
    beginExt = -1;
    endX = -1;
    endY = -1;
    endExt = -1;
    hasRect = false;
    left = -1;
    bottom = -1;
    right = -1;
    top = -1;
    width = 0;
    bool endpath = false;
    do {
      switch (pathId) {
        case odb::dbWireDecoder::PATH:
        case odb::dbWireDecoder::JUNCTION:
        case odb::dbWireDecoder::SHORT:
        case odb::dbWireDecoder::VWIRE:
          layerName = decoder.getLayer()->getName();
          assert(layerNameToIdxs_.find(layerName) != layerNameToIdxs_.end());
          break;
        case odb::dbWireDecoder::POINT:
          if (!hasBeginPoint) {
            decoder.getPoint(beginX, beginY);
            hasBeginPoint = true;
          } else {
            decoder.getPoint(endX, endY);
            hasEndPoint = true;
          }
          beginX = defdist(beginX);
          beginY = defdist(beginY);
          endX = defdist(endX);
          endY = defdist(endY);
          break;
        case odb::dbWireDecoder::POINT_EXT:
          if (!hasBeginPoint) {
            decoder.getPoint(beginX, beginY, beginExt);
            hasBeginPoint = true;
          } else {
            decoder.getPoint(endX, endY, endExt);
            hasEndPoint = true;
          }
          beginX = defdist(beginX);
          beginY = defdist(beginY);
          beginExt = defdist(beginExt);
          endX = defdist(endX);
          endY = defdist(endY);
          endExt = defdist(endExt);

          break;
        case odb::dbWireDecoder::VIA:
          viaName = decoder.getVia()->getName();
          break;
        case odb::dbWireDecoder::TECH_VIA:
          viaName = decoder.getTechVia()->getName();
          break;
        case odb::dbWireDecoder::RECT:
          decoder.getRect(left, bottom, right, top);
          left = defdist(left);
          bottom = defdist(bottom);
          right = defdist(right);
          top = defdist(top);
          hasRect = true;
          break;
        case odb::dbWireDecoder::ITERM:
        case odb::dbWireDecoder::BTERM:
        case odb::dbWireDecoder::RULE:
        case odb::dbWireDecoder::END_DECODE:
          break;
        default:
          break;
      }
      pathId = decoder.next();
      if ((int) pathId <= 3 || pathId == odb::dbWireDecoder::END_DECODE)
        endpath = true;
    } while (!endpath);
  }
  if (hasRect) {
    addBox(layerName, left, bottom, right, top);
  }
  if (hasEndPoint) {
    if (beginX > endX || beginY > endY) {
      std::swap(beginX, endX);
      std::swap(beginY, endY);
      std::swap(beginExt, endExt);
    }
    bool isHorizontal = (beginX != endX);
    if (isHorizontal) {
      addBox(layerName,
             beginX - beginExt,
             beginY - width / 2,
             endX + endExt,
             endY + width / 2);
    } else {
      addBox(layerName,
             beginX - width / 2,
             beginY - beginExt,
             endX + width / 2,
             endY + endExt);
    }
  }
  if (viaName != "") {
    int x, y;
    if (hasEndPoint) {
      x = endX;
      y = endY;
    } else {
      x = beginX;
      y = beginY;
    }
    addVia(viaName, x, y);
  }
}

void OpenDBAdapter::getPinsFromOpenDB()
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  size_t boxIndexBegin = boxes_.size();
  size_t viaIndexBegin = vias_.size();
  for (auto blockage : block->getObstructions()) {
    auto layerName = blockage->getBBox()->getTechLayer()->getName();
    int xl = blockage->getBBox()->xMin();
    int yl = blockage->getBBox()->yMin();
    int xh = blockage->getBBox()->xMax();
    int yh = blockage->getBBox()->yMax();
    addBox(layerName, xl, yl, xh, yh);
  }
  pins_.push_back({copy_string("pin_blockage"),
                   boxIndexBegin,
                   boxes_.size(),
                   viaIndexBegin,
                   vias_.size()});

  auto nets = block->getNets();
  for (auto net : nets) {
    const auto nName = net->getName();
    size_t pinIndexBegin = pins_.size();
    int netIdx = netNameToIdxs_.find(nName)->second;

    // netNameToBTerms.erase(nName);
    // netNameToITerms.erase(nName);

    const auto netBTerms = net->getBTerms();
    for (auto bTerm : netBTerms) {
      boxIndexBegin = boxes_.size();
      viaIndexBegin = vias_.size();

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
      pins_.push_back({copy_string(bTerm->getName()),
                       boxIndexBegin,
                       boxes_.size(),
                       viaIndexBegin,
                       vias_.size()});
    }

    const auto netITerms = net->getITerms();
    for (auto iTerm : netITerms) {
      boxIndexBegin = boxes_.size();
      viaIndexBegin = vias_.size();
      auto inst = iTerm->getInst();
      auto mTerm = iTerm->getMTerm();

      int pX, pY;
      inst->getOrigin(pX, pY);
      odb::Point origin = odb::Point(pX, pY);
      odb::dbTransform transform(inst->getOrient(), origin);

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
      pins_.push_back({copy_string(inst->getName() + "_" + mTerm->getName()),
                       boxIndexBegin,
                       boxes_.size(),
                       viaIndexBegin,
                       vias_.size()});
    }

    if (!net->isSpecial() && net->getWire() != nullptr) {
      boxIndexBegin = boxes_.size();
      viaIndexBegin = vias_.size();
      addWire(net->getWire());

      pins_.push_back({copy_string(nName + "_wire"),
                       boxIndexBegin,
                       boxes_.size(),
                       viaIndexBegin,
                       vias_.size()});
    }

    if (net->isSpecial()) {
      boxIndexBegin = boxes_.size();
      viaIndexBegin = vias_.size();
      for (auto swire : net->getSWires()) {
        for (auto box : swire->getWires()) {
          if (!box->isVia()) {
            addSpecialPath(box);
          } else {
            addVia(box);
          }
        }
      }
      pins_.push_back({copy_string(nName + "_special_wire"),
                       boxIndexBegin,
                       boxes_.size(),
                       viaIndexBegin,
                       vias_.size()});
    }

    nets_.at(netIdx).pin_begin = pinIndexBegin;
    nets_.at(netIdx).pin_end = pins_.size();
  }
}

void OpenDBAdapter::updateDB(banyan_go::entry_ret* ret)
{
}

}  // namespace banyan
