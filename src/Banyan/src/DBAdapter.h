#pragma once

#include <map>

#include "banyan_go.h"
#include "string.h"
#include "utl/Logger.h"

namespace odb {
class dbDatabase;
class dbBlock;
class dbVia;
class dbTechLayer;
class dbNet;
class dbBox;
class dbSBox;
class dbWire;
class dbVia;
class Rect;
}  // namespace odb

namespace utl {
class Logger;
}

namespace banyan {

class DBAdapter
{
 public:
  virtual void updateDB(banyan_go::entry_ret* ret) = 0;
  virtual std::vector<banyan_go::net>& getNets() = 0;
  virtual std::vector<banyan_go::pin>& getPins() = 0;
  virtual std::vector<banyan_go::box>& getBoxes() = 0;
  virtual std::vector<banyan_go::via>& getVias() = 0;
  virtual std::vector<banyan_go::layer>& getLayers() = 0;
  virtual std::vector<banyan_go::viadef>& getViadefs() = 0;
  virtual std::vector<banyan_go::track>& getTracks() = 0;
};

class OpenDBAdapter : public DBAdapter
{
 private:
  std::vector<banyan_go::net> nets_;
  std::vector<banyan_go::pin> pins_;
  std::vector<banyan_go::box> boxes_;
  std::vector<banyan_go::via> vias_;
  std::vector<banyan_go::layer> layers_;
  std::vector<banyan_go::viadef> viadefs_;
  std::vector<banyan_go::track> tracks_;

 protected:
  odb::dbDatabase* db_;
  utl::Logger* logger_;

  bool hasAddedMasterSliceLayer_ = false;
  bool hasAddedCutLayer_ = false;
  int addedLayerIndex_ = 0;
  std::map<std::string, int> layerNameToIdxs_;

  int addedViaDefIndex_ = 0;
  std::map<std::string, int> viaDefNameToIdxs_;

  int addedNetIndex_ = 0;
  std::map<std::string, int> netNameToIdxs_;

  int addedBoxIndex_ = 0;
  int addedViaIndex_ = 0;
  int addedPinIndex_ = 0;

  int defdist(int x);
  int calLayerPitch(odb::dbTechLayer* layer);

  void getLayersFromOpenDB();
  void addRoutingLayer(odb::dbTechLayer* layer);
  void addCutLayer(odb::dbTechLayer* layer);
  void addMasterSliceLayer(odb::dbTechLayer* layer);

  void getTracksFromOpenDB();
  void getViaDefsFromOpenDB();
  void addViaDef(odb::dbVia* odbvia);

  void addBox(odb::dbBox* box);
  void addBox(std::string layerName, const odb::Rect& rect);
  void addBox(std::string layerName, int left, int bottom, int right, int top);

  void addNet(odb::dbNet* net);
  void addWire(odb::dbWire* wire);
  void addSpecialPath(odb::dbSBox* sbox);
  void addVia(odb::dbSBox* box);
  void addVia(std::string viaName, int x, int y);

  void getNetsFromOpenDB();
  void getPinsFromOpenDB();

 public:
  OpenDBAdapter(odb::dbDatabase* db, utl::Logger* logger)
      : db_(db), logger_(logger)
  {
    getLayersFromOpenDB();
    getTracksFromOpenDB();
    getViaDefsFromOpenDB();
    getNetsFromOpenDB();
    getPinsFromOpenDB();
  };

  void updateDB(banyan_go::entry_ret* ret) override;
  std::vector<banyan_go::net>& getNets() override { return nets_; }
  std::vector<banyan_go::pin>& getPins() override { return pins_; }
  std::vector<banyan_go::box>& getBoxes() override { return boxes_; }
  std::vector<banyan_go::via>& getVias() override { return vias_; }
  std::vector<banyan_go::layer>& getLayers() override { return layers_; }
  std::vector<banyan_go::viadef>& getViadefs() override { return viadefs_; }
  std::vector<banyan_go::track>& getTracks() override { return tracks_; }
};

}  // namespace banyan