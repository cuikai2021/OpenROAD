# Sync Up note 20210815

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

    1. rename `opendb` => `odb`
        rename all headers path
        updated CMakeLists.txt
        add a new lib `odbtcl`
        rename `opendbpy` => `odbpy`
        rename all relative functions names
    2. rename `replace` => `gpl`
        updated CMakeLists.txt
        rename all relative functions and filenames
    3. Unified function parameter format and update input arguments' type
    4. add new test cases into regression
    5. rsz remove steiner flute specific setup
    6. grt
        avoid loops when creating wires for checkAntennaViolations
        cleanup Graph::get_level_in_tree()
    7. stt:
        add logger and reporter for steiner tree builder
        add more functions to output tree information
    8. update metric reporter in regression
        update flow_metrics.tcl

## Detail Updates

gui: make helper functions to compute different minimum resolution levels
pdr whittle away until it looks sane
comment
cts max_cap ok
pdr need doSteiner_HoVW
stt/pdrev comments
rsz remove steiner flute specific setup
gui: cache maximum via sizes by layer and use cache to control drawing
dbsta verilog port with alias
gui: check is vias layer has width before rejecting drawning
rcx: simplify routing layer check
rcx: fix var name
rcx: check routing level
Revert "Merge pull request #986 from eder-matheus/grt_revert"
dbsta rm dead code
dbsta Verilog2db::makeDbPins
ibex_sky130hd slew margin
sta missing include <functional>
dbsta write_verilog lsb first port
write_verilog for little endian ports
Fix code style
Move vectors to local scope
Revert "Merge pull request #968 from eder-matheus/grt_update"
rsz: repair_hold10.ok
gpl regression unit test dir name
converting arrays to vectors
report_flow_metric_limits
stt ispd18_sample defok
gpl cmake
stt repair_slew8 ok
rename replace to gpl
grt: ok files
grt: check obstruction layer before processing it
Revert "grt: check for min-max layers before adding obstructions to grid"
stt swig merge fallout
rename OpenDB to odb
grt: check for min-max layers before adding obstructions to grid
stt cleanup tests
grt: fix code style
grt: update ok files
grt: add iteration number
update overflow increases limit
pdr cleanup Graph::get_level_in_tree()
stt: minor refactor
update tests for stt alpha-0.1 fallback
grt: avoid loops when creating wires for checkAntennaViolations
grt: add operator== to GSegment and update constructor
grt region_adjustment guideok
pdr root_idx okfiles
stt: try adding alpha-0.1 fallback
pdr root_index arg finally works
grt: plot actual route guides with highlight_net_route
update tests for stt steiner check
stt pdr change okfile fallout
stt: fix swig wrapper error
stt: use double in write_svg for small tree accuracy
stt: add check test
stt: skip check on high fanout nets
stt: check PD results are a valid Steiner tree
cts: use stt builder instead of call pdII
odb: check that MIMCAP layers are routing before adjusting rlevel
pdr queue distances are floats, not ints
stt flute path depth
stt path depth
gui: switching check for text height outside loop, swap order of checking height limits
gui: viewer clang format names
stt: fix includes
stt: move Tree and Branch structs to SteinerTreeBuilder header
stt regressions report path depth
pdr cleanup
gui: make use of options instance name font
gui: making font selectable in display controls
gui: initial switch to pin instance name to ll corner
gui: rotate instance names when possible, elide name to keep text readable
stt: move methods implementation to cpp file
fix vectors
grt: remote RoutingTracks::HORIZONTAL and ::VERTICAL
odb: update lef_data_access.tcl
odb: routing level equals to zero for MIMCAP layers

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
    [==========] 31 tests from 27 test suites ran. (42 ms total)
    [  PASSED  ] 31 tests.

All test passed

### regression

find regression log here:
[regression.note](./regression.note)

    Passed all tool regressions.
