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
  virtual void updateDB(struct entry_ret* ret) = 0;
  virtual const std::vector<banyan_go::net>& getNets() const = 0;
  virtual const std::vector<banyan_go::pin>& getPins() const = 0;
  virtual const std::vector<banyan_go::box>& getBoxes() const = 0;
  virtual const std::vector<banyan_go::via>& getVias() const = 0;
  virtual const std::vector<banyan_go::layer>& getLayers() const = 0;
  virtual const std::vector<banyan_go::viadef>& getViadefs() const = 0;
  virtual const std::vector<banyan_go::track>& getTracks() const = 0;
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

  void updateDB(struct entry_ret* ret);
  const std::vector<banyan_go::net>& getNets() const { return nets_; }
  const std::vector<banyan_go::pin>& getPins() const { return pins_; }
  const std::vector<banyan_go::box>& getBoxes() const { return boxes_; }
  const std::vector<banyan_go::via>& getVias() const { return vias_; }
  const std::vector<banyan_go::layer>& getLayers() const { return layers_; }
  const std::vector<banyan_go::viadef>& getViadefs() const { return viadefs_; }
  const std::vector<banyan_go::track>& getTracks() const { return tracks_; }
};

}  // namespace banyan