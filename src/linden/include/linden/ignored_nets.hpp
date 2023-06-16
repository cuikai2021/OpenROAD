#pragma once

#include <unordered_set>

namespace odb {
template <class T>
class dbSet;
class dbNet;
class dbITerm;
class dbScanChain;
}

namespace gpl {

class Pin;
class Net;

class IsScanChainsIgnoredPin
{
public:
  explicit IsScanChainsIgnoredPin(const odb::dbSet<odb::dbScanChain>& scan_chains);
  bool operator()(const odb::dbITerm*) const;
private:
  std::unordered_set<const odb::dbITerm*> sc_iterms;
};

class IgnoredNets
{
 public:
  typedef std::unordered_set<odb::dbNet*> NetSet;

  IgnoredNets();

  bool is_clock_net(odb::dbNet*) const noexcept;
  bool is_reset_net(odb::dbNet*) const noexcept;
  bool is_scan_enable_net(odb::dbNet*) const noexcept;

  double get_net_weight(odb::dbNet* net) const noexcept;

  const NetSet& clock_nets() const noexcept { return clock_nets_; }
  const NetSet& reset_nets() const noexcept { return reset_nets_; }
  const NetSet& scan_enable_nets() const noexcept { return scan_enable_nets_; }

 private:
  NetSet clock_nets_;
  NetSet reset_nets_;
  NetSet scan_enable_nets_;

  void init() noexcept;
};


bool is_high_fanout_net(odb::dbNet*) noexcept;

}  // namespace gpl