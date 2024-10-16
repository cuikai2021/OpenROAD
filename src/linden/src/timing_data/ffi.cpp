#include "ffi.hpp"
#include "pb_tdp_data.hpp"
#include "ord/OpenRoad.hh"

namespace linden {

void write_tdp_data(const char* filename) {
  rsz::TdpDataWriter writer(ord::OpenRoad::openRoad());
  writer.serialize(filename);
}

void read_tdp_data(const char* filename) {
  rsz::TdpDataWriter writer(ord::OpenRoad::openRoad());
  writer.deserialize(filename);
}

} // namespace linden
