#pragma once

#include <string>
#include <variant>
#include <deque>

#include "odb/geom.h"
#include "banyan_cts.pb.h"

namespace sta {
class dbNetwork;
}  // namespace sta

namespace cts {
class ClockInst;

using banyan::RouteTree;

class RouteTreeBuilder {
public:
	void setClkInst(ClockInst* ci);
	void setMergePoint(odb::Point point);
	void ClkInst2PinName(sta::dbNetwork* _network, bool is_src = true);
	RouteTreeBuilder& addSubtree();
	RouteTree* toPb() const;

	RouteTreeBuilder() = default;
	~RouteTreeBuilder() = default;
private:
	void setPinName(std::string pin_name);

	// invaild / pin-name / merge-point
	std::variant<ClockInst*, std::string, odb::Point> data;
	std::deque<RouteTreeBuilder> subtrees;
};

}  // namespace cts
