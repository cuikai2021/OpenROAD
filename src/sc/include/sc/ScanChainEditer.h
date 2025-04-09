#pragma once

#include <memory>
namespace odb {
  class dbITerm;
  class dbNet;
} // namespace odb

namespace sc {
namespace sc_editer {

class ScanChainEditerBase {
public:
  virtual ~ScanChainEditerBase() = default;
  virtual void connect(odb::dbITerm* iterm, odb::dbNet* net) = 0;
  virtual void disconnect(odb::dbITerm* iterm) = 0;
};

class ScanChainEditerDB : public ScanChainEditerBase {
public:
  void connect(odb::dbITerm* iterm, odb::dbNet* net) override ;
  void disconnect(odb::dbITerm* iterm) override ;
};


using EditerCreator = std::shared_ptr<ScanChainEditerBase>(*)();

void setEditerCreator(EditerCreator creater);
std::shared_ptr<ScanChainEditerBase> editer();

} // namespace sc_editer
} // namespace sc