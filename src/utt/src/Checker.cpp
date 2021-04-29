#include <Checker.h>

namespace utt {

inline std::ifstream init_istream(std::string path)
{
  std::ifstream infile;
  infile.open(path);
  return infile;
}

inline bool isblank(std::string str, ignore ig)
{
  for (unsigned int i = 0; i < str.size(); ++i) {
    if (str[i] != ' ') {
      if (ig != no_ignore) {
        if (ig == ignore_commit_script && str[i] == '#') {
          return true;
        }
        if (ig == ignore_commit_code && i < str.size() - 1 && str[i] == '/'
            && str[i + 1] == '/') {
          return true;
        }
      }
      return false;
    }
  }
  return true;
}

void file_equal(std::string file0, std::string file1, ignore ig)
{
  std::string s0 = {};
  std::string s1 = {};
  std::ifstream if0 = init_istream(file0);
  std::ifstream if1 = init_istream(file1);

  while (std::getline(if0, s0)) {
    if (!isblank(s0, ig)) {
      while (std::getline(if1, s1) && isblank(s1, ig))
        ;
      if (s0.compare(s1)) {
        FAIL() << ":\n(file0: " << file0 << ")\n"
               << s0 << "\n(file1:" << file1 << ")\n"
               << s1;
        break;
      }
    }
  }
}

}  // namespace utt