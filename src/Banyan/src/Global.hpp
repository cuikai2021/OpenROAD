#pragma once
#include "pprint.hpp"

namespace banyan {
extern pprint::PrettyPrinter printer;

enum class BanyanLogLevel
{
  TRACE,
  DEBUG,
  INFO
};

}  // namespace banyan