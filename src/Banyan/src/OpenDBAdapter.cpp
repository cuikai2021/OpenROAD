
#include <assert.h>

#include <utility>

#include "DBAdapter.h"
#include "opendb/db.h"
#include "utl/Logger.h"

namespace banyan {

char* copy_string(std::string in_str)
{
  char* out_str = new char[in_str.length() + 1];
  in_str.copy(out_str, in_str.length());
  out_str[in_str.length()] = '\0';
  return out_str;
}

size_t calLastIndex(int index)
{
  return size_t(index == 0 ? index : index - 1);
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
  assert(addedLayerIndex_ > 1);
  assert(hasAddedMasterSliceLayer_ && hasAddedCutLayer_);
  if (layer->getPitchX() != 0) {
    layers_.push_back(
        {(int) layer->getWidth(), layer->getPitchX(), 0, 0, 0, 0});
  } else {
    layers_.push_back(
        {(int) layer->getWidth(), layer->getPitchY(), 0, 0, 0, 0});
  }
  layerNameToIdxs_.insert(make_pair(layer->getName(), addedLayerIndex_));
  addedLayerIndex_++;
}

void OpenDBAdapter::addCutLayer(odb::dbTechLayer* layer)
{
  assert(addedLayerIndex_ > 0);
  assert(hasAddedMasterSliceLayer_);

  layers_.push_back({(int) layer->getWidth(), layer->getPitch(), 0, 0, 0, 0});
  layerNameToIdxs_.insert(make_pair(layer->getName(), addedLayerIndex_));
  addedLayerIndex_++;
  hasAddedCutLayer_ = true;
}

void OpenDBAdapter::addMasterSliceLayer(odb::dbTechLayer* layer)
{
  assert(addedLayerIndex_ == 0);
  assert(!hasAddedMasterSliceLayer_ && !hasAddedCutLayer_);

  if (layer->getLef58Type() != odb::dbTechLayer::LEF58_TYPE::NWELL
      && layer->getLef58Type() != odb::dbTechLayer::LEF58_TYPE::PWELL
      && layer->getLef58Type() != odb::dbTechLayer::LEF58_TYPE::DIFFUSION) {
    layers_.push_back({0, 0, 0, 0, 0, 0});
    layerNameToIdxs_.insert(make_pair(layer->getName(), addedLayerIndex_));
    addedLayerIndex_++;
    hasAddedMasterSliceLayer_ = true;
  }
}

void OpenDBAdapter::getTracksFromOpenDB()
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  auto tracks = block->getTrackGrids();
  std::map<std::string, std::vector<odb::dbTrackGrid*>> tracksGroupByLayerName;
  for (auto track : tracks) {
    std::string lname = track->getTechLayer()->getName();
    if (tracksGroupByLayerName.count(lname) == 0) {
      std::vector<odb::dbTrackGrid*> groupedTracks;
      groupedTracks.push_back(track);
      tracksGroupByLayerName.insert(make_pair(lname, groupedTracks));
    } else {
      tracksGroupByLayerName.find(lname)->second.push_back(track);
    }
  }

  int trackIndex = 0;
  for (auto& [layerName, tracks] : tracksGroupByLayerName) {
    int layerIdx = layerNameToIdxs_.find(layerName)->second;
    int laTrackBegin = trackIndex;

    for (auto track : tracks) {
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
    layers_.at(layerIdx).track_end = calLastIndex(trackIndex);
  }
}

void OpenDBAdapter::getViaDefsFromOpenDB()
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  if (block->getVias().size() == 0) {
    return;
  }

  std::map<std::string, std::vector<odb::dbVia*>> viasGroupByTopLayerName;
  for (auto via : block->getVias()) {
    std::string lname = via->getTopLayer()->getName();
    if (viasGroupByTopLayerName.count(lname) == 0) {
      std::vector<odb::dbVia*> groupedVias;
      groupedVias.push_back(via);
      viasGroupByTopLayerName.insert(make_pair(lname, groupedVias));
    } else {
      viasGroupByTopLayerName.find(lname)->second.push_back(via);
    }
  }

  for (auto& [layerName, bTerms] : viasGroupByTopLayerName) {
    int layerIdx = layerNameToIdxs_.find(layerName)->second;

    int viaDefIndexBegin = addedViaDefIndex_;
    for (auto via : block->getVias()) {
      addViaDef(via);
    }

    layers_.at(layerIdx).viadef_begin = viaDefIndexBegin;
    layers_.at(layerIdx).viadef_end = calLastIndex(addedViaDefIndex_);
  }
}

void OpenDBAdapter::addViaDef(odb::dbVia* via)
{
  std::string vname = via->getName();

  int botLayerIdx
      = layerNameToIdxs_.find(via->getBottomLayer()->getName())->second;
  int topLayerIdx
      = layerNameToIdxs_.find(via->getTopLayer()->getName())->second;

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
    std::map<std::string, std::set<odb::dbBox*>> layerNameToBoxes;

    for (auto box : via->getBoxes()) {
      std::string lname = box->getTechLayer()->getName();
      if (layerNameToBoxes.count(lname) == 0) {
        std::set<odb::dbBox*> groupedBoxes;
        layerNameToBoxes.insert(make_pair(lname, groupedBoxes));
      } else {
        layerNameToBoxes.find(lname)->second.insert(box);
      }
    }

    int cnt = 0;

    for (auto& [layerName, boxes] : layerNameToBoxes) {
      for (auto box : boxes) {
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
  viadefs_.push_back({copy_string(vname), botBox, topBox});
  viaDefNameToIdxs_.insert(make_pair(vname, addedViaDefIndex_));
  addedViaDefIndex_++;
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
  nets_.push_back({copy_string(net->getName()), 0, 0, 0, 0, 0, 0});
  netNameToIdxs_.insert(make_pair(net->getName(), addedNetIndex_));
  addedNetIndex_++;
}

void OpenDBAdapter::addBox(odb::dbBox* box)
{
  int layerIdx = layerNameToIdxs_.find(box->getTechLayer()->getName())->second;
  boxes_.push_back({layerIdx,
                    defdist(box->xMin()),
                    defdist(box->yMin()),
                    defdist(box->getDX()),
                    defdist(box->getDY())});
  addedBoxIndex_++;
}

void OpenDBAdapter::addVia(odb::dbBox* box)
{
  odb::dbTechVia* via = box->getTechVia();
  if (via == nullptr) {
    via = box->getBlockVia()->getTechVia();
  }
  int viaDefIdx = viaDefNameToIdxs_.find(via->getName())->second;
  vias_.push_back({viaDefIdx, box->xMin(), box->yMin()});
  addedViaIndex_++;
}

void OpenDBAdapter::getPinsFromOpenDB()
{
  odb::dbBlock* block = db_->getChip()->getBlock();
  std::map<std::string, std::vector<odb::dbBTerm*>> bTermsGroupByNetName;
  for (auto bTerm : block->getBTerms()) {
    std::string nname = bTerm->getNet()->getName();
    if (bTermsGroupByNetName.count(nname) == 0) {
      std::vector<odb::dbBTerm*> groupedBTerms;
      groupedBTerms.push_back(bTerm);
      bTermsGroupByNetName.insert(make_pair(nname, groupedBTerms));
    } else {
      bTermsGroupByNetName.find(nname)->second.push_back(bTerm);
    }
  }
  for (auto& [netName, bTerms] : bTermsGroupByNetName) {
    int pinIndexBegin = addedPinIndex_;
    int netIdx = netNameToIdxs_.find(netName)->second;
    for (auto bTerm : bTerms) {
      int boxIndexBegin = addedBoxIndex_;
      int viaIndexBegin = addedViaIndex_;
      for (auto bPin : bTerm->getBPins()) {
        for (auto box : bPin->getBoxes()) {
          if (box->isVia()) {
            addVia(box);
          } else {
            addBox(box);
          }
        }
      }
      pins_.push_back({copy_string(bTerm->getName()),
                       (size_t) boxIndexBegin,
                       (size_t) calLastIndex(addedBoxIndex_),
                       (size_t) viaIndexBegin,
                       (size_t) calLastIndex(addedViaIndex_)});
      addedPinIndex_++;
    }
    nets_.at(netIdx).pin_begin = pinIndexBegin;
    nets_.at(netIdx).pin_end = calLastIndex(addedPinIndex_);
  }
}

void OpenDBAdapter::updateDB(struct entry_ret* ret)
{
}

}  // namespace banyan