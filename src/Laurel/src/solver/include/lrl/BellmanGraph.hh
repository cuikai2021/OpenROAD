#pragma once

#include <functional>
#include <set>

namespace lrl {

template <class T, typename Value, class Compare = std::less<Value>>
class BellmanGraph
{
 public:
  typedef int32_t Index;
  typedef std::pair<Index, Index> IdEdge;
  typedef std::pair<IdEdge, Value> IdEdgeWeight;

  void addWeight(IdEdgeWeight const& w) { graph_.insert(w); }
  bool solve(Value const& init_value, size_t max_iter)
  {
    // Bellman Ford Shortest Path Algorithm
    init(init_value);
    for (size_t i = 0; i <= max_iter; ++i)
      if (!updateArrivalMap())
        return true;
    return false;
  }

  size_t size() const { return index_.size(); }
  Index& operator[](T const& t) { return index_[t]; }
  Index& at(T const& t) const { return index_.map.at(t); }
  void forEach(std::function<void(T, Index)> const& f) const
  {
    for (auto& i : index_.map)
      f(i.first, i.second);
  }
  inline Value const& result(Index const& i) const { return fixed_value_[i]; }
  inline Value const& result(T const& t) const { return result(index_.map.at(t)); }
  void clearGraph() { graph_.clear(); }

 protected:
  struct IndexMap
  {
    typedef std::unordered_map<T, Index> Map;
    Index& operator[](T const& v) { return map.insert({v, size()}).first->second; }
    size_t size() const { return map.size() + NodeIndex::t_index + 1; }
    Map map;
  } index_;

  void init(Value const& v) { fixed_value_.resize(index_.size(), v); }
  bool updateArrivalMap()
  {
    bool changed = false;
    Compare less;
    for (auto& edge : graph_) {
      Index const& pred = edge.first.first;
      Index const& succ = edge.first.second;
      if (succ == NodeIndex::s_index || succ == NodeIndex::t_index)
        continue;
      Value& succ_value = fixed_value_[succ];
      Value new_value = fixed_value_[pred] + edge.second;
      if (less(succ_value, new_value)) {
        succ_value = new_value;
        changed = true;
      }
    }
    return changed;
  }

  typedef std::set<IdEdgeWeight> Graph;
  Graph graph_;
  std::vector<Value> fixed_value_;
};

}  // namespace lrl
