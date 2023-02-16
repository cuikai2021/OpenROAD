#include <iostream>
#include "db.h"
#include "definScanchain.h"
#include "utl/Logger.h"

namespace odb {

static dbInst*
findInst(dbBlock* blk, utl::Logger* _logger, const char* name) {
  dbInst* inst = nullptr;
  std::string n = name;
  size_t pos = 0;
  while (!(inst = blk->findInst(n.c_str()))) {
	 pos = n.find('/', pos);
	 if (pos == -1) break;
	 n[pos] = '.';
  }
  if (!inst)
    _logger->warn(utl::ODB, 3001, "error: cannot find instance: %s", name);
  inst->setSizeOnly(true);
  return inst;
}


static dbITerm*
findTerm(dbInst* inst, utl::Logger* _logger, const char* pinname) {
  dbITerm* t = inst->findITerm(pinname);
  if (!t)
	 _logger->warn(utl::ODB, 3002, "error: cannot find instance with pin: %s/$s", inst->getName(), pinname);
  return t;
}

static dbITerm*
findTerm(dbBlock* blk, utl::Logger* _logger, const char* instname, const char* pinname) {
  dbInst* i = findInst(blk, _logger, instname);
  if (!i) return nullptr;
  return findTerm(i, _logger, pinname);
}

static dbITerm*
findTerm(dbBlock* blk, utl::Logger* _logger, const char* name) {
  std::string n = name;
  size_t pos = n.rfind('/');
  std::string instname = n.substr(0, pos);
  std::string pinname = n.substr(pos + 1);
  return findTerm(blk, _logger, instname.c_str(), pinname.c_str());
}

void
definScanchains::end() {
  definScanchain* scR = scanchains.back();
  dbScanChain* sc = dbScanChain::create(_block, scR->name.c_str(),
	 scR->commonInPin.c_str(), scR->commonOutPin.c_str(),
	 !scR->partitionName.empty() ? scR->partitionName.c_str() : NULL,
	 scR->partitionMaxBits);

  if (!scR->start.inst_name.empty()) {
    dbITerm* start = nullptr;
    if (!scR->start.pin_name.empty())
      start = findTerm(_block, _logger, scR->start.inst_name.c_str(), scR->start.pin_name.c_str());
    else
      start = findTerm(_block, _logger, scR->start.inst_name.c_str());
    sc->setStart(start);
  }
  if (!scR->stop.inst_name.empty()) {
    dbITerm* stop = nullptr;
    if (!scR->stop.pin_name.empty())
	   stop = findTerm(_block, _logger, scR->stop.inst_name.c_str(), scR->stop.pin_name.c_str());
    else
	   stop = findTerm(_block, _logger, scR->stop.inst_name.c_str());
    sc->setStop(stop);
  }
  if (scR->floating)
  for (auto & x: scR->floating->list) {
    dbInst* inst = findInst(_block, _logger, x.inst_name.c_str());
    if (!inst) continue;
    std::string ipin_name = x.in_pin_name;  if (ipin_name.empty()) ipin_name = scR->commonInPin.c_str();
    dbITerm* iterm = findTerm(inst, _logger, ipin_name.c_str());
    if (!iterm) continue;
    std::string opin_name = x.out_pin_name; if (opin_name.empty()) opin_name = scR->commonOutPin.c_str();
    dbITerm* oterm = findTerm(inst, _logger, opin_name.c_str());
    if (!oterm) continue;
    sc->createFloatingComp(inst, iterm, oterm, x.bits);
  }
  for (auto & y: scR->ordered) {
    dbScanChainList* ordered = sc->createOrderedList();
    for (auto & x: y->list) {
    	dbInst* inst = findInst(_block, _logger, x.inst_name.c_str());
		if (!inst) continue;
		std::string ipin_name = x.in_pin_name;  if (ipin_name.empty()) ipin_name = scR->commonInPin.c_str();
		dbITerm* iterm = findTerm(inst, _logger, ipin_name.c_str());
		if (!iterm) continue;
		std::string opin_name = x.out_pin_name; if (opin_name.empty()) opin_name = scR->commonOutPin.c_str();
		dbITerm* oterm = findTerm(inst, _logger, opin_name.c_str());
		if (!oterm) continue;
		ordered->createComp(inst, iterm, oterm, x.bits);
    }
  }
}

void
definScanchains::scanchainsEnd() {
  for (auto & x : scanchains) {
	 delete x;
  }
  scanchains.clear();
}

} // end namespace odb
