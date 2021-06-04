#pragma once

#include <map>


#include "banyan_go.h"
#include "string.h"


namespace odb {
class dbDatabase;
class dbBlock;
class dbVia;
class dbTechLayer;
class dbNet;
}

namespace utl {
class Logger;
}

namespace banyan{

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
  std::map<std::string,int> layerNameToIdxs_;

  int addedBoxIndex_ = 0;

  int addedViaDefIndex_ = 0;
  std::map<std::string,int> viaDefNameToIdxs_;

  int addedNetIndex_=0;
  std::map<std::string,int> netNameToIdxs_;



  int defdist(int x);


  void getLayersFromOpenDB();
  void addRoutingLayer(odb::dbTechLayer* layer);
  void addCutLayer(odb::dbTechLayer* layer);
  void addMasterSliceLayer(odb::dbTechLayer* layer);

  void getTracksFromOpenDB();
  void getViaDefsFromOpenDB();
  void addViaDef(odb::dbVia* odbvia);

  void addBox(banyan_go::box box);
  void addNet(odb::dbNet* net);


  void getNetsFromOpenDB();
  void getPinsFromOpenDB();

  
 public:
  OpenDBAdapter(odb::dbDatabase* db, utl::Logger* logger):db_(db),logger_(logger){
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


}  // namespace