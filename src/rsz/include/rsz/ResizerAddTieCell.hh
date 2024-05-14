#pragma once

#include "rsz/Resizer.hh"
#include "sta/Network.hh"

namespace rsz {

using utl::RSZ;

class ResizerAddTieCell : public Resizer
{
 public:
  ResizerAddTieCell() = delete;
  /// cname: cell name
  /// pname: port/pin name
  void addTieCell(const char* tie_lo_cname,
                  const char* tie_lo_pname,
                  const char* tie_hi_cname,
                  const char* tie_hi_pname);

  void removeTieCell(const char* tie_lo_cname,
                     const char* tie_lo_pname,
                     const char* tie_hi_cname,
                     const char* tie_hi_pname);

 protected:
  std::pair<LibertyCell*, LibertyPort*> findLibertyCellAndPort(
      const char* cell_name,
      const char* port_name);

  Instance* makeInstance(LibertyCell* cell,
                         const char* name,
                         Instance* parent,
                         const Point& loc,
                         odb::dbSourceType type);
  
  std::vector<Net*> getNameSortedNets();

  // Sort OBJs by the dictionary order of their names.
  template <typename OBJ>
  std::deque<OBJ*> staIter2StableDeque(sta::Iterator<OBJ*>* iter)
  {
    std::deque<OBJ*> ret;
    while (iter->hasNext()) {
      auto val = iter->next();
      ret.push_back(val);
    }
    std::sort(ret.begin(), ret.end(), [this](OBJ* l, OBJ* r) {
      return strcmp(network_->name(l), network_->name(r)) < 0;
    });
    return ret;
  }

};
static_assert(sizeof(ResizerAddTieCell) == sizeof(Resizer), "Since we want split addTieCell from Resizer, we get ResizerAddTieCell from Resizer cast. So just keep class ResizerAddTieCell same memory layout with Resizer");
}