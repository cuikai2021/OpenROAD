#include <itest.h>

namespace testing {

bool compareFiles(const std::string& p1, const std::string& p2)
{
  std::ifstream f1(p1, std::ifstream::binary | std::ifstream::ate);
  std::ifstream f2(p2, std::ifstream::binary | std::ifstream::ate);

  if (f1.fail() || f2.fail()) {
    return false;  // file problem
  }

  if (f1.tellg() != f2.tellg()) {
    return false;  // size mismatch
  }

  // seek back to beginning and use std::equal to compare contents
  f1.seekg(0, std::ifstream::beg);
  f2.seekg(0, std::ifstream::beg);
  return std::equal(std::istreambuf_iterator<char>(f1.rdbuf()),
                    std::istreambuf_iterator<char>(),
                    std::istreambuf_iterator<char>(f2.rdbuf()));
}

void file_equal(const std::string file0, const std::string file1, bool need_equal)
{
  std::string msg = "two files are different";

  if (!(compareFiles(file0, file1) != need_equal))
  {
    if(!need_equal){
      msg = "two files are same";
    }
    FAIL() << msg;
  }
}

}  // namespace testing