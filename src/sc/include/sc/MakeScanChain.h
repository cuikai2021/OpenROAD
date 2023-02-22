#pragma once

namespace sc {
class ScanChain;
}  // namespace sc

namespace ord {

class OpenRoad;

sc::ScanChain* makeScanChain();

void deleteScanChain(sc::ScanChain* scanChain);

void initScanChain(OpenRoad* openroad);

}  // namespace ord
