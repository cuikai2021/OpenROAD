# Sync Up note 20210727

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

1. add new cmd `write_abstract_lef`
2. update DependencyInstaller.sh
    add `libreadline-dev` via apt
3. add new module `stt` (steiner tree builder) into `ord`
    refactor
4. remove module `pdr`
5. update `OpenDB`
    add polymorphism for `dbGroupType::dbGroupType`
    remove `boost` usage in `OpenDB` and use `boostParser.h` as replacement
6. formatter
7. update FlexDR
    add `getDesign` back to FlexDR.h
    refactor `draw edges` of `draw graph` in `draw layer`
8. optimize cts' log and arguments, and remove redundant codes
9. update test cases

## Detail Updates

    ppl: add arguments names
    ppl: const&
    stt: use const&
    ppl: ok files
    ppl: print only hpwl after pin placement
    ppl: printf hpwl in microns
    ppl: print hpwl for non-random runs
    ppl: remove deprecated variables
    stt: fanout = term_count -1
    stt: restore degree parameter to flute functions
    grt: update flute functions
    stt: update flute to use std::vector instead of arrays
    iterator mismatch bugfix
    docker: add readline as a dev dependency
    more compiler warning cleanups
    misc compiler warnings cleanup
    stt: fix uninitialized members
    stt: fix array copy
    grt: fix uninitialized member
    odb: compiler warning cleanup (#917)
    adding missing Exception.i to ensure runtime errors are caught by swig
    Format the max_merge_res value for RCX message 436 to account for platform differences. Seen 0 (versus 0.0) appear which can cause regression to fail
    stt: minor cleanups
    ppl: remove copy of vectors
    ppl: update HungarianMatching
    ppl: add pin indices instead of a netlist to sections
    grt: update pd4 test with new message
    gui: do not create Selected on ITerm when not connected to a net
    const refactoring for PR
    changes for PR
    gui: cycle through selections when there are overlapping choices
    Update Logger.rst
    gui: removing arguments for functions for the parent items
    gui: switching to use isCover
    odb: adding isCover to master and mastertype
    gui: break makeItem into two
    Update Logger.rst
    odb: ifFill() == CORE SPACER and nothing else
    gui: fix newline in nets
    gui: adding CallbackFunction to displayControls
    stt: fix structs
    stt: add inst/pin name in error message
    gui: add comment about non-null argument for selection boxes
    gui: add comment about non-null argument for selection boxes
    mpl: fix memory error
    msg update
    rsz re-ignore specialnet
    stt/pdrev bulletproof against zero pin nets
    gui: correcting handling of ignore callback
    Wirebond -> wirebond
    fixup diff
    centre -> center in doc
    gui: adding selectable checkbox for nets
    stt: check if terms are placed
    Revert ""
    stt: simplify checks
    update msgs
    grt/stt: more unit tests
    stt: add option to set alpha per min hpwl
    grt/stt: unit test
    stt: add option to set alpha per min fanout
    gui: switch to use openDB calls for is* on instance viewing and selecting
    gui: adding endcap instance group
    gui: fix clang format
    Update Logger.rst
    add cd command
    gui: adding controls for instances
    gui: disabled selectable column when visible is is turned off
    gui: adding drawing and selection limits to instances
    gui: adding instances to options.h
    fix issues in msgs
    remove c_str() call
    update run-ispd.tcl
    pdr 2 duplicate points
    gui: switch to 5x minimum resolution for draw rows and tracks
    Revert "gui: draw outline of rows when zoomed out"
    Revert "gui: correcting row outline loop and selection of rows for outline"
    gui: correcting row outline loop and selection of rows for outline
    gui: correct handling of site height and width for vertical rows
    fixed syntax
    - changed grid graph up edge creation policy for restricted routing cases - add special treatment to on-the-fly ap creation for restricted routing cases
    rsz: update test results for filler exclusion from util
    gui: draw outline of rows when zoomed out
    gui: only draw rows and sites in visible area and selectively draw site vs. rows depending on resolution
    gui: change drawTracks to only draw over block, and to skip drawing if the steps of tracks is less than the minimum resolution
    gui: adding resolution helper function
    rsz: exclude fillers from area/utilization calculation
    Re-wording
    improve jenkins email report
    fix indentation
    fix broken link
    add _build to gitignore
    fix doc syntax
    update ok files
    style
    use logger
    Update Logger.rst
    Update Logger.rst
    rsz missing target slews with multiple corners OR-1072
    Fixing syntax error in the LEF file.
    Update Logger.rst
    improve text
    grt: fix coverity issues
    sta: update submodule
    Revert "sta: update submodule"
    sta: update submodule
    ppl: update var name
    make OR compile with boost 1.76
    Update Logger.rst
    stt: flute fixes
    rsz: remove grt
    add Metrics.rst to docs
    Update Logger.rst
    Update Logger.rst
    stt: update pdr api
    stt: refactor
    stt: more fixes
    stt: small fixes
    gui: providing feedback to display controls to partially check parent

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

    both modified:   src/ICeWall/src/ICeWall.tcl
        <line 499~538, accept current changes>
        <line 612~654, accept current changes>
    both modified:   src/tap/src/tapcell.tcl
        <line 156~161, accept current changes>

### fix bugs after merge

    [SyncUp] remove `pdr` in utEden cmake to adapt new structure

### gtest

find unit test log here:
[ut.note](./ut.note)

    [----------] Global test environment tear-down
    [==========] 31 tests from 27 test suites ran. (43 ms total)
    [  PASSED  ] 31 tests.

All test passed

### regression

find regression log here:
[regression.note](./regression.note)

    Passed all tool regressions.
