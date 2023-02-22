%{

#include "sc/ScanChain.h"

namespace ord {
// Defined in OpenRoad.i
    sc::ScanChain *
        getScanChain();
}

using ord::getScanChain;

%}

%include "../../Exception.i"

%inline %{

namespace sc {

    void
    reorder_cmd()
    {
      sc::ScanChain *scanChain = getScanChain();
      return scanChain->reorder();
    }

    void
    trace_cmd()
    {
      sc::ScanChain *scanChain = getScanChain();
      return scanChain->trace();
    }

    void
    report_scan_chain(const char *filename)
    {
      sc::ScanChain *scanChain = getScanChain();
      return scanChain->reportScanChain(filename);
    }
} // namespace

%} // inline
