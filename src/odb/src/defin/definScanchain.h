#pragma once

#include <string>

#include <map>
#include <vector>

#include "definBase.h"
#include "odb.h"

namespace odb {

class definScanchain : public definBase {
public:
  struct Pin {
    std::string inst_name;
	 std::string pin_name;
   };
  struct Comp {
    std::string inst_name;
    std::string in_pin_name;
    std::string out_pin_name;
    int         bits = 1;
  };
  struct CompList {
	 CompList(size_t sz) : list() { list.reserve(sz); }
	 void add(char* inst, char* inPin, char* outPin, int bits) {
		 list.emplace_back(Comp{inst, inPin, outPin, bits});
	 }
	 std::vector<Comp> list;
  };
  definScanchain(const char* n) : name(n), floating(NULL), partitionMaxBits(0) {}
  ~definScanchain() {
	 for (auto & o:ordered)
		delete o;
    delete floating;
  }
  void setStart(char* inst, char* pin) {
    start.inst_name = inst;
    if (pin) start.pin_name  = pin;
  }
  void setStop(char* inst, char* pin) {
    stop.inst_name = inst;
    if (pin) stop.pin_name  = pin;
  }
  CompList* createOrdered(size_t size) {
	 CompList* ret = new CompList(size);
	 ordered.push_back(ret);
	 return ret;
  }
  CompList* createFloating(size_t size) {
	 floating = new CompList(size);
	 return floating;
  }
  void setCommonInPin(const char* p) { commonInPin = p; }
  void setCommonOutPin(const char* p) { commonOutPin = p; }
  void setPartitionName(const char* p) { partitionName = p; }
  void setPartitionMaxBits(int v) { partitionMaxBits = v; }

  std::string            name;
  Pin                    start;
  Pin                    stop;
  std::vector<CompList*> ordered;
  CompList*              floating;
  std::string            commonInPin;
  std::string            commonOutPin;
  std::string            partitionName;
  int                    partitionMaxBits;
};

class definScanchains : public definBase {
public:
	definScanchain* begin(const char* name) {
		definScanchain* d = new definScanchain(name);
		scanchains.push_back(d);
		return d;
	}
	void end();
	void scanchainsBegin(int n) {
		assert(scanchains.empty());
		number = n;
	}
	void scanchainsEnd();

	size_t number;
	std::vector<definScanchain*> scanchains;
};

} // end namespace odb
