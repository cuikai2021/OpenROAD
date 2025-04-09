#include "sc/ScanChainEditer.h"

#include "odb/db.h"

namespace sc {
namespace sc_editer {

// these are function under flatten mode, not safe under hierarchical mode.

void
ScanChainEditerDB::connect(odb::dbITerm* iterm, odb::dbNet* net) {
  iterm->connect(net);
}

void
ScanChainEditerDB::disconnect(odb::dbITerm* iterm) {
  iterm->disconnect();
}


static std::shared_ptr<ScanChainEditerBase>
scanChainEditerDBCreater() {
  return std::make_shared<ScanChainEditerDB>();
}

static
EditerCreator _editer = nullptr;

void
setEditerCreator(EditerCreator creater) {
  _editer = creater;
}

std::shared_ptr<ScanChainEditerBase>
editer() {
  if (_editer == nullptr) setEditerCreator(scanChainEditerDBCreater);
  return _editer();
}

} // namespace sc_editer
} // namespace sc