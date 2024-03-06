#pragma once

#include <string>

namespace ord {

class OpenRoad;

} // namespace ord

namespace rsz {

class Resizer;

class IncrTdpDataWriter {
 public:
  IncrTdpDataWriter() = delete;
  IncrTdpDataWriter(ord::OpenRoad* o): ord_(o) {}
  virtual ~IncrTdpDataWriter() = default;

  // write down pb file into specified path
  bool serialize(std::string const& file_name);

  // debug, report the data but do not store anything.
  bool deserialize(std::string const& file_name);

 protected:
  ord::OpenRoad* ord_ = nullptr;
 private:
  // not allowed to be copied
  IncrTdpDataWriter(IncrTdpDataWriter const&) = delete;
  IncrTdpDataWriter& operator=(IncrTdpDataWriter const&) = delete;
};

} // namespace rsz