#pragma once

#include <deque>
#include <optional>
#include <string>

namespace ord {
class OpenRoad;
}

namespace linden {
namespace dp {

// dump lef/def files to /tmp directory for linden::dp to use
class LefDefDumper
{
 public:

  void run(::ord::OpenRoad& ord);

  const std::string& basename() const noexcept { return _basename; }
  const std::string& tech_lef() const noexcept { return _tech_lef; }
  const std::deque<std::string>& lib_lefs() const noexcept { return _lib_lefs; }
  const std::string& def() const noexcept { return _def; }

  std::ostream& print_dumped_files(std::ostream& oss) const;

 private:
  std::string _basename;
  std::string _tech_lef;
  std::deque<std::string> _lib_lefs;
  std::string _def;
};

}  // namespace dp
}  // namespace linden