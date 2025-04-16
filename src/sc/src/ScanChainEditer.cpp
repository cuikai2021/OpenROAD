#include "sc/ScanChainEditer.h"

#include "odb/db.h"

namespace sc {
namespace sc_editer {

using utl::SC;

static
EditerCreator _editer = nullptr;

// Assume it will be a small list only contains several elements.
static
std::vector<ScanChainEditerCallBackBase*> _editer_cbs;

void
ScanChainEditerNotifyNetSwitchCallBack::removeNetBefore(odb::dbNet* n) {
  logger_->info(SC, 11, "Remove Net {}", n->getConstName());
}

void
ScanChainEditerNotifyNetSwitchCallBack::createNetAfter(odb::dbNet* n) {
  logger_->info(SC, 11, "Create Net {}", n->getConstName());
}

// these are function under flatten mode, not safe under hierarchical mode.

ScanChainEditerBase::ScanChainEditerBase() {
  callbacks_ = &_editer_cbs;
}

void
ScanChainEditerDB::connect(odb::dbITerm* iterm, odb::dbNet* net) {
  iterm->connect(net);
  connectAfter(iterm, net);
}

void
ScanChainEditerDB::disconnect(odb::dbITerm* iterm) {
  auto net = iterm->getNet();
  iterm->disconnect();
  if (net) disconnectAfter(iterm, net);
}


static std::shared_ptr<ScanChainEditerBase>
scanChainEditerDBCreater() {
  return std::make_shared<ScanChainEditerDB>();
}

void
setEditerCreator(EditerCreator creater) {
  _editer = creater;
}

void
registerCallBack(ScanChainEditerCallBackBase *callback) {
  if (callback == nullptr) return;
  auto searched = std::ranges::find(_editer_cbs, callback);
  if (searched == _editer_cbs.end()) {
    _editer_cbs.push_back(callback);
  }
}

void
unregisterCallBack(ScanChainEditerCallBackBase *callback) {
  if (callback == nullptr) return;
  auto searched = std::ranges::find(_editer_cbs, callback);
  if (searched != _editer_cbs.end()) {
    _editer_cbs.erase(searched);
  }
}

std::shared_ptr<ScanChainEditerBase>
editer() {
  if (_editer == nullptr) setEditerCreator(scanChainEditerDBCreater);
  return _editer();
}

} // namespace sc_editer
} // namespace sc