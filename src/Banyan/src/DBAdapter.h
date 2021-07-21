#pragma once

#include <map>
#include <list>

#include "banyan_go.h"
#include "string.h"
#include "utl/Logger.h"

namespace odb {
class dbDatabase;
class dbBlock;
class dbObject;
class dbTechLayer;
class dbNet;
class dbBox;
class dbSBox;
class dbWire;
class dbVia;
class dbTechVia;
class dbTechNonDefaultRule;
class Rect;
}  // namespace odb

namespace utl {
class Logger;
}

namespace banyan {

class DBAdapter
{
 public:
  virtual void updateDB(std::vector<banyan_go::layer>& layers,
                        std::vector<banyan_go::net>& nets,
                        std::vector<banyan_go::viadef>& viadefs,
                        banyan_go::route_ret& ret)
      = 0;
  virtual std::vector<banyan_go::net>& getNets() = 0;
  virtual std::vector<banyan_go::pin>& getPins() = 0;
  virtual std::vector<banyan_go::box>& getBoxes() = 0;
  virtual std::vector<banyan_go::via>& getVias() = 0;
  virtual std::vector<banyan_go::layer>& getLayers() = 0;
  virtual std::vector<banyan_go::viadef>& getViadefs() = 0;
  virtual std::vector<banyan_go::track>& getTracks() = 0;
  virtual std::vector<banyan_go::ndr_layer>& getNDRLayers() = 0;
  virtual std::vector<banyan_go::ndr>& getNDRs() = 0;
  virtual banyan_go::design getDesign() = 0;

  virtual ~DBAdapter() {}
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
  std::vector<banyan_go::ndr_layer> ndrlayers_;
  std::vector<banyan_go::ndr> ndrs_;
  banyan_go::design design_;

 protected:
  odb::dbDatabase* db_;
  utl::Logger* logger_;

  bool hasAddedMasterSliceLayer_ = false;
  bool hasAddedCutLayer_ = false;
  std::map<std::string, int> layerNameToIdxs_;

  std::map<std::string, int> viaDefNameToIdxs_;

  std::map<std::string, size_t> netNameToIdxs_;

  std::map<std::string, size_t> ndrNameToIdxs_;
  std::list<std::string> strs_;

  int defdist(int x);
  int calLayerPitch(odb::dbTechLayer* layer);

  void getDesignFromOpenDB();

  void getLayersFromOpenDB();
  void addRoutingLayer(odb::dbTechLayer* layer);
  void addCutLayer(odb::dbTechLayer* layer);
  void addMasterSliceLayer(odb::dbTechLayer* layer);

  void getTracksFromOpenDB();
  void getViaDefsFromOpenDB();
  void addViaDef(odb::dbObject* odbvia);

  void getNDRsFromOpenDB();
  void addNDR(odb::dbTechNonDefaultRule* ndr);

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
  void getInstsObsFromOpenDB();


 public:
  OpenDBAdapter(odb::dbDatabase* db, utl::Logger* logger)
      : db_(db), logger_(logger)
  {
    getDesignFromOpenDB();
    getLayersFromOpenDB();
    getTracksFromOpenDB();
    getViaDefsFromOpenDB();
    getNDRsFromOpenDB();
    getNetsFromOpenDB();
    getPinsFromOpenDB();
    getInstsObsFromOpenDB();
  }

  ~OpenDBAdapter() {}

  void updateDB(std::vector<banyan_go::layer>& layers,
                std::vector<banyan_go::net>& nets,
                std::vector<banyan_go::viadef>& viadefs,
                banyan_go::route_ret& ret) override;
  std::vector<banyan_go::net>& getNets() override { return nets_; }
  std::vector<banyan_go::pin>& getPins() override { return pins_; }
  std::vector<banyan_go::box>& getBoxes() override { return boxes_; }
  std::vector<banyan_go::via>& getVias() override { return vias_; }
  std::vector<banyan_go::layer>& getLayers() override { return layers_; }
  std::vector<banyan_go::viadef>& getViadefs() override { return viadefs_; }
  std::vector<banyan_go::track>& getTracks() override { return tracks_; }
  std::vector<banyan_go::ndr_layer>& getNDRLayers() override { return ndrlayers_; }
  std::vector<banyan_go::ndr>& getNDRs() override { return ndrs_; }
  banyan_go::design getDesign() override { return design_; }
};

}  // namespace banyan