# Sync Up note 20210809

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

1.update README.md
2.update pdn
    add new tcl cmd: `add_global_connection`, `define_pdn_grid`
    add new regressions
4.add new docker release mode `binary`
5.optimize ICeWall.tcl
    removed annoying tmp file `cover.def` from ICeWall output.
    resolve `rdl_layer` and do `odb::dbSBox_create`
6.TritonRoute: remove or merge unused functions
7.optimized stt and fix bugs

## Detail Updates

set_propagated_clock before estimate_parasitics
rsz 100k max pin limit
pdr::node rm pointless array inits
rsz estimate_parasitics not on ideal clock nets
gui: fix clang warnings about return values
stt: add flute unit tests
stt: add helpers to report flute tree
stt: fix flute for 1 pin net
stt: refactor and fix computeHPWL check
Remove unused, incorrect files
Clean regression
Use correct mapping of LEF orientations, plus some documentation updates
gui: check if congestion map is empty before getting x and y vectors
gui: check if congestion map is on before processing anything
stt: fix return type
stt: default min_hpwl and min_fanout is 0
stt: restore pt and ptp sizes, fix stable_sort call
stt: fix checks
Check for LEF orientations, converting them to geometric orientations to check against instance orientation
Reinstate PR#834 by reverting "Revert "Merge pull request #834 from The-OpenROAD-Project-staging/pdngen-tcl_api""
stt: check min_fanout and min_hpwl before compute it for each net
stt: fix pt and ptp sizes
stt: fix comp function for stable_sort
gui: only modify button name if not specified, report error if duplicate name is used
gui: limit instance names to width of cell
gui: obstruction adding effective width and min spacing properties
gui: clang format brush selection button
grt: more cleanups
Revert "Merge pull request #834 from The-OpenROAD-Project-staging/pdngen-tcl_api"
rsz repair_design handle 2 overlapping pin net
gui: adding tcl commands to gui
gui: adding ability to request user input via gui
gui: support for creating + removing new buttons
gui: modifying script widget to pass command for execution + control for echo of command
gui: adding support to change and save instance name colors
gui: adding blockage and obstructions
gui: draw instance names on instance + separate function for drawing instance outlines
gui: display controls saving and reading simplified
gui: seperate making swatch icon
gui: correct behavior of parent toggling
gui: remove fixed size from color dialog + size radio button to ensure non-overlap of brush style
Update required to testcase data to work with PR#806
grt: add layer name in error message
pdn: temporarily make error into warning until a proper fix is available
rsz repair_design to output port
grt: use reference instead of pointer
grt: fix uninitialized vars
grt: DTYPE -> int
grt: update struct definitions
grt: use enum class
grt: use stt::Tree
grt: remove deprecated functions
grt: remove redundant Tree and Branch structures
Write RDL routes directly to the database
grt: more cleanups
odb: ensure bbox is set on inst when swapping masters
gui: adding tcl commands for clear selection, rules, highlights, and modifying display controls
gui: adding ability to save to image from layout viewer and support tcl commands to access from commandline
grt: minor code cleanups in maze.cpp
grt: add comment for resizing check
updated rmp message
grt: add check for negative resize
gui: fix addInstTransform on MX90 and MY90
grt: clang-format
grt: resize initializing with zero
Correct typo

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

    no bugs need to be fixed

### gtest

find unit test log here:
[ut.note](./ut.note)

    [----------] Global test environment tear-down
    [==========] 31 tests from 27 test suites ran. (57 ms total)
    [  PASSED  ] 31 tests.
All test passed

### regression

find regression log here:
[regression.note](./regression.note)

    Passed all tool regressions.
