
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
  assert(layers_.size() > 1);
  assert(hasAddedMasterSliceLayer_ && hasAddedCutLayer_);
  auto it = layerNameToIdxs_.insert(make_pair(
      layer->getName(), (int)layers_.size())).first;
  layers_.push_back({it->first.c_str(),
      (int) layer->getWidth(), calLayerPitch(layer), 0, 0, 0, 0});
}

void OpenDBAdapter::addCutLayer(odb::dbTechLayer* layer)
{
  assert(layers_.size() > 0);
  assert(hasAddedMasterSliceLayer_);

  layerNameToIdxs_.insert(make_pair(layer->getName(), -(int)layers_.size()));
  hasAddedCutLayer_ = true;
}

void OpenDBAdapter::addMasterSliceLayer(odb::dbTechLayer* layer)
{
  assert(layers_.empty());
  assert(!hasAddedMasterSliceLayer_ && !hasAddedCutLayer_);

  if (layer->getLef58Type() != odb::dbTechLayer::LEF58_TYPE::NWELL
      && layer->getLef58Type() != odb::dbTechLayer::LEF58_TYPE::PWELL
      && layer->getLef58Type() != odb::dbTechLayer::LEF58_TYPE::DIFFUSION) {
    auto it = layerNameToIdxs_.insert(make_pair(layer->getName(), 0)).first;
    layers_.push_back({it->first.c_str(), 0, 0, 0, 0, 0, 0});
    hasAddedMasterSliceLayer_ = true;
  }
}

void OpenDBAdapter::getTracksFromOpenDB()
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  const auto tracks = block->getTrackGrids();
  for (auto group :
       utl::group_by(tracks.begin(), tracks.end(), TrackLayerName())) {
    int layerIdx = layerNameToIdxs_.find(group.value)->second;
    if (layerIdx < 0) {
      logger_->info(utl::BY,
              800,
              "ignore tracks on layer[{}] {}",
              layerIdx, group.value);
      continue;
    }

    size_t laTrackBegin = tracks_.size();
    std::vector<int> grid;
    int dir = banyan_go::DIR_NONE, tmin = 0, tmax = 0;
    for (auto track : group.range) {
      if (track->getTechLayer()->getDirection() == odb::dbTechLayerDir::HORIZONTAL) {
        track->getGridY(grid);
        dir = banyan_go::DIR_HORIZONTAL;
        tmin = design_.xmin;
        tmax = design_.xmax;
        break;
      }
      else if (track->getTechLayer()->getDirection() == odb::dbTechLayerDir::VERTICAL) {
        track->getGridY(grid);
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
              layerIdx, group.value);
    }

    for (auto line: grid) {
      tracks_.push_back({layerIdx, line, tmin, tmax});
    }

    layers_.at(layerIdx).dir = dir;
    layers_.at(layerIdx).track_begin = laTrackBegin;
    layers_.at(layerIdx).track_end = tracks_.size();
  }
}

void OpenDBAdapter::getViaDefsFromOpenDB()
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  if (block->getVias().size() == 0) {
    return;
  }
  const auto vias = block->getVias();

  for (auto group : utl::group_by(vias.begin(), vias.end(), ViaTopLayer())) {
    int layerIdx = layerNameToIdxs_.find(group.value)->second;
    if (layerIdx < 0) {
      logger_->info(utl::BY,
              800,
              "ignore viadefs on layer[{}] {}",
              layerIdx, group.value);
      continue;
    }

    size_t viaDefIndexBegin = viadefs_.size();
    for (auto via : group.range) {
      addViaDef(via);
    }

    layers_.at(layerIdx).viadef_begin = viaDefIndexBegin;
    layers_.at(layerIdx).viadef_end = viadefs_.size();
  }
}

void OpenDBAdapter::addViaDef(odb::dbVia* via)
{
  std::string vname = via->getName();

  int botLayerIdx
      = layerNameToIdxs_.find(via->getBottomLayer()->getName())->second;
  int topLayerIdx
      = layerNameToIdxs_.find(via->getTopLayer()->getName())->second;
  if ((botLayerIdx < 0) || (topLayerIdx < 0)) {
    logger_->error(utl::BY,
            801,
            "invalid viadef {} layers {} {}",
            vname, topLayerIdx, botLayerIdx);
    throw odb::ZException("invalid viadef layers");
  }

  struct banyan_go::box botBox;
  struct banyan_go::box topBox;
  odb::dbTechViaGenerateRule* rule = via->getViaGenerateRule();
  if (via->hasParams() && (rule != nullptr)) {
    odb::dbViaParams P;
    via->getViaParams(P);

    int xSize = defdist(P.getXCutSize());
    int ySize = defdist(P.getYCutSize());
    int xCutSpacing = defdist(P.getXCutSpacing());
    int yCutSpacing = defdist(P.getYCutSpacing());
    int xOrigin = defdist(P.getXOrigin());
    int yOrigin = defdist(P.getYOrigin());
    int xTopEnc = defdist(P.getXTopEnclosure());
    int yTopEnc = defdist(P.getYTopEnclosure());
    int xBotEnc = defdist(P.getXBottomEnclosure());
    int yBotEnc = defdist(P.getYBottomEnclosure());
    int xTopOffset = defdist(P.getXTopOffset());
    int yTopOffset = defdist(P.getYTopOffset());
    int xBotOffset = defdist(P.getXBottomOffset());
    int yBotOffset = defdist(P.getYBottomOffset());

    int currX = 0;
    int currY = 0;
    for (int i = 0; i < P.getNumCutRows(); i++) {
      currX = 0;
      for (int j = 0; j < P.getNumCutCols(); j++) {
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
    const auto boxes = via->getBoxes();
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
  auto it = viaDefNameToIdxs_.insert(make_pair(
    vname, (int)viadefs_.size())).first;
  viadefs_.push_back({it->first.c_str(), botBox, topBox});
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
  auto it = netNameToIdxs_.insert(make_pair(
    net->getName(), nets_.size())).first;
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
      throw odb::ZException("unsupported NET USE");
  }
  nets_.push_back({it->first.c_str(), use, 0, 0, 0, 0, 0, 0});
}

void OpenDBAdapter::addBox(odb::dbBox* box)
{
  int layerIdx = layerNameToIdxs_.find(box->getTechLayer()->getName())->second;
  if (layerIdx < 0) {
    //logger_->info(utl::BY,
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
    //logger_->info(utl::BY,
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
    //logger_->info(utl::BY,
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
            layerIdx, box->getTechLayer()->getName());
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
  pins_.push_back({"obstructions",
                   boxIndexBegin, boxes_.size(),
                   viaIndexBegin, vias_.size()});

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
      strs_.emplace_back(bTerm->getName());
      pins_.push_back({strs_.back().c_str(),
                       boxIndexBegin, boxes_.size(),
                       viaIndexBegin, vias_.size()});
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
      strs_.emplace_back(inst->getName() + "/" + mTerm->getName());
      pins_.push_back({strs_.back().c_str(),
                       boxIndexBegin, boxes_.size(),
                       viaIndexBegin, vias_.size()});
    }

    if (!net->isSpecial() && net->getWire() != nullptr) {
      boxIndexBegin = boxes_.size();
      viaIndexBegin = vias_.size();
      addWire(net->getWire());

      strs_.emplace_back(nName + "/wire");
      pins_.push_back({strs_.back().c_str(),
                       boxIndexBegin, boxes_.size(),
                       viaIndexBegin, vias_.size()});
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
      strs_.emplace_back(nName + "/specialwire");
      pins_.push_back({strs_.back().c_str(),
                       boxIndexBegin, boxes_.size(),
                       viaIndexBegin, vias_.size()});
    }

    nets_.at(netIdx).pin_begin = pinIndexBegin;
    nets_.at(netIdx).pin_end = pins_.size();
  }
}

void OpenDBAdapter::updateDB(banyan_go::entry_ret* ret)
{
}

}  // namespace banyan
