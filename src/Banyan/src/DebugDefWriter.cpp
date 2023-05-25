#include "DebugDefWriter.hpp"

#include <fstream>

namespace banyan {

void DebugDefWriter::addBox(const std::string& layer, const ByBox& b)
{
  std::unique_lock<std::mutex> lock(mtx_);
  char buf[1000];
  std::sprintf(buf,
               "- LAYER %s RECT ( %d %d ) ( %d %d ) ;\n",
               layer.c_str(),
               b.xMin(),
               b.yMin(),
               b.xMax(),
               b.yMax());
  text_.emplace_back(buf);
}
void DebugDefWriter::addBox(const std::string& layer, std::shared_ptr<ByBox>& b)
{
  addBox(layer, *b);
}
void DebugDefWriter::addBoxes(const std::string& layer,
                              const std::vector<ByBox>& boxes)
{
  for (const auto& b : boxes) {
    addBox(layer, b);
  }
}
void DebugDefWriter::addBoxes(const std::string& layer,
                              const std::vector<std::shared_ptr<ByBox>>& boxes)
{
  for (const auto& b : boxes) {
    addBox(layer, *b);
  }
}
bool DebugDefWriter::write(const std::string& path)
{
  std::unique_lock<std::mutex> lock(mtx_);
  std::fstream file;
  file.open(path, std::ios::out);  // write,清空再写入
  if (file.is_open()) {
    file << head_;
    std::sort(text_.begin(), text_.end());
    for (auto& s : text_)
      file << s;
    file << tail_;
    return true;
  }
  return false;
}
void DebugDefWriter::init()
{
  std::unique_lock<std::mutex> lock(mtx_);
  text_.clear();
  head_ = "DESIGN CORTEXM3 ;\n";
  char buf[256];
  std::sprintf(
      buf, "DIEAREA ( %d %d ) ( %d %d ) ;\n", xMin_, yMin_, xMax_, yMax_);
  head_ += std::string(buf);
  head_ += "BLOCKAGES 1 ;\n";
  tail_ = "END BLOCKAGES\nEND DESIGN\n";
}
void DebugDefWriter::addPolygon(const std::string& layer,
                                const bsp::polygon_90_data<int>& pl)
{
  BsPolygon90Set s;
  s.insert(pl);
  addPolygonSet(layer, s);
}
void DebugDefWriter::addPolygonSet(const std::string& layer,
                                   const BsPolygon90Set& s)
{
  std::vector<ByBox> ans;
  s.get_rectangles(ans);
  addBoxes(layer, ans);
}
void DebugDefWriter::addInfo(const std::string& info)
{
  std::unique_lock<std::mutex> lock(mtx_);
  text_.emplace_back(info + "\n");
}
}  // namespace banyan
