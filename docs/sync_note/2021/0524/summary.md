# Sync Up note 20210524

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

    1. delete dead codes
    2. add new flow and new test
    3. fixed some bugs of gui, tap, grt, par
    4. add new functions for ppl, drt
    5. optimized functions in drt, ppl

## Detail Updates

    drt: fix assert
    drt: fix non-gui error
    drt: add inspector for grid graph nodes/edges
    [DRT] fix typo: bottom --> top

    ppl: new unit test
    ppl: check pin placement status for constraints
    ppl: remove wrong break
    ppl: new unit tests for random pin placement
    ppl: add getValidSlots function
    ppl: respect constrained pin groups during random placement
    ppl: respect pin groups during random placement

    dpl obstruction
    dpl return error count
    
    
    check double ndr definition
    added reading NDR from block

    ifp first row orient R0
    ifp okfiles


    grt: simplify std::max for l2v
    grt: fix coverity issue
    grt: fix overflow message
    grt: clang-format
    grt: remove dead code
    grt: remove deprecated function

    tap: update ok files
    tap: update check_if_filled
    tap: fix cells at top/bottom of the core
    tap: fix typo
    tap: fix east/west corner cells

    par: fix partition solutions from getting pinned to wrong metric

    gui: fix non-gui build missing symbols

    Rewrite the label to push action.
    Update drv limits
    internal power for port->port default duty cycle
    required changes    
    tinyRocket n45 slack
    gcd n45 slack
    flow nangate45 rcx support
    demo CM
    add .whitespace file for testing workflow
    flow write metrics, report_flow_metrics.tcl
    flow sideline ibex_sky130hs because rcx seg faults
    gpl simpleminded01-td
    flow enable rcx
    cleanup compiler warnings
    Update run-ispd.tcl
    Update FlexDR_graphics.h
    test cm label

## debug information

### build

find build log here:
[build.note](./build.note)

Eden can be built successfully.
utEden can be built successfully.

### git

find git log here:
[git.note](./git.note)

#### Conflicts

    src/ppl/include/ppl/IOPlacer.h      (line 153~154) accept input but remind haoyuan modification 0419

#### fix bugs after merge

    no bugs need to be fixed

### gtest

find unit test log here:
[ut.note](./ut.note)

    [----------] Global test environment tear-down
    [==========] 38 tests from 29 test suites ran. (28 ms total)
    [  PASSED  ] 38 tests.

All test passed

### regression

find regression log here:
[regression.note](./regression.note)

    Passed all tool regressions.
