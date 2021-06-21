# Sync Up note 20210621

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

    1. rename part sst to stt
    2. drt refactor FlexDRWorker and replace `design_` from `tech` and `via_data`
        use `getTech()` directly to replace `getDesign()->getTech()`

    3. debug and optimize in gui, grt, pdr, ppl

    4. add `message` function for each part
        Now openRoad will check every error message in the tcl scripts of each parts
        If there are errors have same index, the compilation will be stopped.
        This might need us to check the index before we add a new error message in tcl scripts.

## Detail Updates

    fix compiler error + warning
    revert use std namespace
    fix boundary path seg merging improved conn error debugging

    - remove truncatedStyle checks
    doc remove duplicated instructions
    gui: break adding to output into seperate functions and provide python style command line continuation
    Update GettingStarted.rst
    gui: only add new lines to commands if they are incomplete
    gui: remove output prefix and only add prefix to first line of command
    gui: use unique pointer for context menu items
    fix flow repo name
    grt: refactor iterm->getNet()
    grt: revert unnecessary changes
    change minWidth dataType
    fix overflow
    copyrights
    Update run-ispd.tcl
    grt: ignore special nets when detecting dirty nets
    pdr: adding missing limits include
    pdr lala test
    compare_flow_metrics
    compare_flow_metrics
    pdr unsigned BE GONE!
    docs: fix local install
    docs: fix title
    docs: clarify
    docs: remove duplicated file
    rsz est rc seg fault if no driver
    pdr magic number cleanup
    pdr cleanup
    pdr prim-dijkstra star spanning tree -> 2^N run time fix
    Check that nets/bterms are not NULL

    - clang-format
    - via ext calculation back to the default method (but corrected now) - updated .defok
    tap: fix messages
    grt: fix messages
    ppl: fix messages
    update messages.txt
    adding context menu option to enable/disable syntax highlighting
    provide a little extra margin in the command line height
    switching from QVector to std::vector for unique_ptr's
    gui: adding syntax highlighting for tcl and openroad commands
    gui: add support to drag and drop file into command input area
    gui: adding support for multi-line input commands
    gui: adding support function to add lines to output and setting max line length
    pdr debug msg cleanup
    clang-format
    pdr spanning tree gui view
    handle ICeWall messages.txt specially
    odb: make auto messages.txt work
    dbSta: update results for message id fix
    Automate the building of the messages.txt files
    pdr comment
    compare_flow_metrics
    Update max_skew_violations 909 -> 914
    clang-format + change location of graphics_->show()
    update test results for msg duplicate fixes
    cleanup duplicate message ids
    ibex max cap metric
    gpl simple01-td ok
    rsz improve overlapping pin support
    fix merge error
    change ord::error to utl::error
    find_messages.py fix unicode error
    make etc/find_messages.py executable
    Replace etc/FindMessages.tcl with etc/find_messages.py
    rsz est rc for clocks (again)
    fixed compiler errors
    revert VIAINPIN_DEFAULT_BOTTOM_LAYER check and constant
    pdr cleanup round 2
    pdr Dikstra -> Dijkstra, add papers
    rsz pd issue
    Add newline

    - MTSAFEDIST now considering ndr spacing - GC special rects always queried now
    pdr test with duplicate xy
    constexpr in constant
    ppl: use static_cast
    Fix usage of pdrev
    Give warning instead of error in case no clocks are found
    pdrev -> pdr
    flute rename lib and dir from sst to stt
    pdrev rm copies of flute structs
    revert clang in TritonRoute.i
    review changes
    pdrev rewrite interface to use functions
    gui: ignore mouse events before block is initialized
    pdrev use int instead of unsigned
    clang-format
    pdrev cleanup round 1
    pdrev unit test
    logger do not limit debugging prints
    via-in-pin 2nd fix
    Fix for `Dockerfile.runtime` , Docker helper script
    Update GettingStarted.rst
    Update GettingStarted.rst
    drt: remove design dependency from FlexDRWorker::route_queue
    drt: clarify design dependencies in gc
    fix DRC on special spacing NDR rectangles on
    VIAINPIN flag fix
    ppl: update messages.txt
    ppl: add check to avoid division by zero
    ppl: fix uninitialized struct member
    support EOLEXT in TritonRoute

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

    both modified:   etc/DependencyInstaller.sh     <line 60, accept incoming change, add `$SUDO` above>
    both modified:   src/mpl/CMakeLists.txt         <line 67~70, accept incoming change>
    both modified:   src/tap/src/tapcell.tcl        <line 1054, accept incoming change>

#### fix bugs after merge

    [SyncUp] re-number duplicate warning indexes
    [SyncUp] DRT fit remove of `design_` in `FlexDRWorker`
    [SyncUp] fix CMakeLists.txt to fit eden project and utest
    [SyncUp] update compilation message.txt

### gtest

find unit test log here:
[ut.note](./ut.note)

    [----------] Global test environment tear-down
    [==========] 55 tests from 37 test suites ran. (4387 ms total)
    [  PASSED  ] 55 tests.

All test passed

### regression

find regression log here:
[regression.note](./regression.note)

    Passed all tool regressions.
