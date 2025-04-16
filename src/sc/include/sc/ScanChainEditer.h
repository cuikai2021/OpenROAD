#pragma once

#include <list>
#include <memory>
#include <vector>
namespace odb {
  class dbITerm;
  class dbNet;
} // namespace odb

namespace sta {
  class Net;
  class Pin;
}

namespace utl {
  class Logger;
}

namespace sc {
namespace sc_editer {

class ScanChainEditerCallBackBase {
public:
  virtual ~ScanChainEditerCallBackBase() = default;
  virtual void removeNetBefore(odb::dbNet* n) {}
  virtual void removeNetAfter(odb::dbNet* n) {}
  virtual void createNetBefore(odb::dbNet* n) {}
  virtual void createNetAfter(odb::dbNet* n) {}
  virtual void disconnectAfter(odb::dbITerm* p, odb::dbNet* n) {}
  virtual void connectAfter(odb::dbITerm* p, odb::dbNet* n) {}
};

class ScanChainEditerNotifyNetSwitchCallBack : public ScanChainEditerCallBackBase {
public:
  ScanChainEditerNotifyNetSwitchCallBack(utl::Logger* log) : logger_(log) {}
  void removeNetBefore(odb::dbNet* n) override;
  void createNetAfter(odb::dbNet* n) override;
protected:
  utl::Logger* logger_;
};

class ScanChainEditerBase {
public:
  ScanChainEditerBase();
  virtual ~ScanChainEditerBase() = default;
  virtual void connect(odb::dbITerm* iterm, odb::dbNet* net) = 0;
  virtual void disconnect(odb::dbITerm* iterm) = 0;

  void removeNetBefore(odb::dbNet* n) {
    for (auto cb : *callbacks_) cb->removeNetBefore(n);
  }
  void createNetAfter(odb::dbNet* n) {
    for (auto cb : *callbacks_) cb->createNetAfter(n);
  }
  void disconnectAfter(odb::dbITerm* p, odb::dbNet* n) {
    for (auto cb : *callbacks_) cb->disconnectAfter(p, n);
  }
  void connectAfter(odb::dbITerm* p, odb::dbNet* n) {
    for (auto cb : *callbacks_) cb->connectAfter(p, n);
  }
protected:
  std::vector<ScanChainEditerCallBackBase*>* callbacks_;
};

class ScanChainEditerDB : public ScanChainEditerBase {
public:
  void connect(odb::dbITerm* iterm, odb::dbNet* net) override ;
  void disconnect(odb::dbITerm* iterm) override ;
};

using EditerCreator = std::shared_ptr<ScanChainEditerBase>(*)();

void setEditerCreator(EditerCreator creater);
void registerCallBack(ScanChainEditerCallBackBase* callback);
void unregisterCallBack(ScanChainEditerCallBackBase *callback);
std::shared_ptr<ScanChainEditerBase> editer();

} // namespace sc_editer
} // namespace sc