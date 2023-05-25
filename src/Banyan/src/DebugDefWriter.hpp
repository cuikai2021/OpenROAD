#pragma once
#include <string>
#include <vector>
#include <mutex>
#include "ByBox.hpp"

namespace banyan {
class DebugDefWriter
{
 public:
  DebugDefWriter()
  {
    xMax_ = yMax_ = 100000;
    init();
  }
  DebugDefWriter(int xMin, int yMin, int xMax, int yMax)
      : xMin_(xMin), yMin_(yMin), xMax_(xMax), yMax_(yMax)
  {
    init();
  }
  DebugDefWriter(int xMax, int yMax) : xMax_(xMax), yMax_(yMax) { init(); }
  ~DebugDefWriter() = default;
  void addBox(const std::string& layer, const ByBox& b);
  void addBox(const std::string& layer, std::shared_ptr<ByBox>& b);
  void addBoxes(const std::string& layer, const std::vector<ByBox>& boxes);
  void addBoxes(const std::string& layer,
                const std::vector<std::shared_ptr<ByBox>>& boxes);
  void addInfo(const std::string& info);
  void addPolygon(const std::string& layer,
                  const bsp::polygon_90_data<int>& pl);
  void addPolygonSet(const std::string& layer, const BsPolygon90Set& s);
  bool write(const std::string& path);
  void init();

 private:
  int xMin_{}, yMin_{}, xMax_{}, yMax_{};
  std::string head_, tail_;
  std::vector<std::string> text_;
  std::mutex mtx_;
};
}  // namespace banyan
