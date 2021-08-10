# Sync Up note 20210802

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

1.update log format and regression logs' format
2.optimized `opendDB`
    add building exception `unknown def version` in openDB `.i` file for python parser
    remove redundant return value of function `::create()`
3.optimized `TritonRoute`
    [DRT] Cap droute_end_iter at 64
4.update and optimize `gui`
5.ifp: fix double to int rounding in meterToDbu
6.fix compiler bug in cts
    defined `targetDist` while `#ifndef NDEBUB`
    so Revert "remove assert which forget to be removed"

## Detail Updates

repair_clock_net ignore special nets
gui: resize highlight and select views to content
gui: correct columns in inspector
gui: correct columns in displayControls
[DRT] Cap droute_end_iter at 64
gui: make iterm limit static + clang
gui: limit iterms to keep gui responsive
cts: fix compiler error in debug mode
rcx: cleanup many compiler warnings
Fix typos for command names Fix typo to use timing instead of delay in option "target"
Fix undefined behavior with sprintf.
use std::round instead of adding 0.5
misc compiler warning cleanup
ifp: fix double to int rounding in meterToDbu
pad: correcting calls to logger

## debug information

### build

find build log here:
[logBuild.note](./logBuild.note)

Eden can be built successfully.
utEden can be built successfully.

### git

find git log here:
[git.note](./git.note)

#### Conflicts

    no conflicts

#### fix bugs after merge

    Revert "remove assert which forget to be removed"

### gtest

find unit test log here:
[ut.note](./ut.note)

    [----------] Global test environment tear-down
    [==========] 31 tests from 27 test suites ran. (49 ms total)
    [  PASSED  ] 31 tests.

All test passed

### regression

find regression log here:
[regression.note](./regression.note)

    Passed all tool regressions.
