# Sync Up note 20210718

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

1. **add new module `rmp` (restructure)**
    Restructuring could be done targeting area or timing
2. **rename TritonCTS as `cts`, Partition as `par`**
3. **add a `mpl2` (rtl_macro_placer) which is included in mpl**
4. **ICeWall**:
    add new arguments for cmd `set_bump_options`:
    `-offset {x_offset y_offset}` and `-array_size {num_rows num_cols}`
    rename arguments for cmd `set_padring_options`:
    `-pad_inst_name pad_inst_name` => `-pad_inst_pattern pad_inst_pattern`
    `-pad_pin_name pad_pin_name` => `-pad_pin_pattern pad_pin_pattern`
5. **openDB**:
    add more getters for `dbObject` and `dbOrientType`
    remove `dbITerm::print` function
    add judgement `==` between rhs at different layers(high or low)
6. **cts**:
    remove `StaEngine` and move (and optimized) its functions into `TritonCTS`
    optimize constructor of `CtsOption` and `TechChar` and clear redundant codes
    DBU is int, not long long
    simplify liberty cap access
7. **grt**:
    update constructor
    fix check for wrong-way layer assignment
    avoid assign edges to invalid layers
    update maze3D to be aware of net layer range
    refactor structure of codes

## Detail Updates

Revert "Merge pull request #857 from eder-matheus/grt_layer_restriction"
tcl: update comment relating to history.tcl
tcl: handle error from tcl history initial load centrally
clang-format + update drv limits (ispd18_test4: 9 -> 5)
rsz div by zero snake bite
grt: update constructor
grt: fix check for wrong-way layer assignment
grt: minor update
grt: fix constructor and destructor
cts delete block sta's after use
grt: update metrics
grt: update ok files
grt: avoid assign edges to invalid layers
gui: add successful commands to tcl history during session
rsz corral buffers inside core
refactorings
Revert "cts max_cap test"
cts max_cap test
grt: get clocks first
cts rm more worthless messages
grt: refactor check
grt: small update
cts rm setSta call
grt: remove deprecated functions
grt: update ok files
grt: add layer range information to FastRouteCore
grt: update maze3D to be aware of net layer range
grt: add min-max layers to FrNet
cts rm DBU
cts rm DBU partial
cts misspelling
cts rm pointless class StaEngine
cts DBU is int, not long long
cts factor and name magic number 200
cts messages
cts rm references to openroad singlton
cts cleanup
cts rm reinvention of std::round (mingyu school of programming)
cts rm reinvention of std::round (mingyu school of programming)
cts elmore delays wrong
cts rm more opensta use nonsense
cts cleanup rc init
remove on-the-fly ap temp fix
implemented connectivity inference by route obj info (truncate style for path seg, bools for vias)
ppl: add check for manufacturing grid
rsz: fix sign error in hold buffering
cts remove useless db/chip/block/sta checks
cts don't use '#' in messages
gui: fix wrong bounds check in congestion display
cts more info cleanup
odb: start to commonize swig between python & tcl
cts define MAX_NORMALIZED_VAL fromNUM_BITS_PER_FIELD
cts rm progress announcement blocks
cts rm duplicate test check_wire_rc_cts_2
Changes to cache level buffers to query during reporting.
ibex sky130hs slew/cap margin
flow metrics
grt: fix code style
grt: use range-for
grt: move const to function
grt: arrays to vectors/multi_array again
grt: return const ref&
grt: add enum for RouteType
grt: remove defines
grt: reorganize class members
rsz respect set_routing_alpha
grt: minor fixes
cts characterization simplify parasitics calls
grt: more arrays to vectors + fix deleteComponents
cts simplify liberty cap access
cts remove pointless characterization sdc
grt: move global vars to FastRouteCore class
grt: move utility functions to FastRouteCore class
grt: remove unnecessary includes
grt: fix coverity issues
grt: move RipUp functions to FastRouteCore class
cts correct some of the many opensta apt abuses
cts rm silly use of std::pow
CodingPractices
Update metrics.
destroy malformed rules in OpenDB
Fix jenkins failure
Update golds for remove_buffer changes for port connected buffers.
Correct boolean operation for port buffer flags.
grt: move route functions to FastRouteCore class
grt: move maze3D functions to FastRouteCore class
grt: move RSMT functions to FastRouteCore class
grt: move DataType.h to include/
grt: move maze functions to FastRouteCore class
grt: remove ADIFF, use abs instead
Update set_bump_options.md
odb: remove unneeded dbITerm::print
remove debugging lines
support eolkeepout exceptwithin
grt: move EdgeShift functions to utility
grt: remove dead code
grt: small fixes
grt: remove useless ()
grt: turn arrays into boost::multi_array
grt: clang-format
Remove buffer connected to single port as well.
findLogicConstants init const func outputs
grt: turn arrays into vectors
handle messages split over multiple lines when generating messages.txt
flow metrics (worst with rd than td)
grt: remove more deprecated variables
flow use routing driven gpl
grt: restore metrics
grt: remove messages
grt: add max_connections const for heights and eID arrays size
grt: typedef Bool -> bool
rmp: disable yosys-abc dependent tests gcd_restructure & const_cell_removal
add rmp to overall regression
grt: remove deprecated files
grt: remove global vars 2nd round
rmp: fix copyright notice holder
rmp: add missing copyright notices
rmp: minor formatting of blifParser.h
rmp: clang-format and add eol at eof
restore include MakeTritoncts.h mistakenly removed
remove messages.txt from abc_remap branch
mpl2: update mp_test1.ok
grt: remove global vars 1st round
updated defok file
Update mp_test1 config and tcl file
Update regression test result
regression disable mpl2 tests because LOAD_PARTITIONS not on
ICeWall bug fixes, addition of set_bump commands, plsu extra arguments for set_bump_options command
grt: fix undefined behavior in coeffADJ
exclude rmp
Add rmp in regression
par: remove unused using/includes
par: fix old iteration style
PartitionMgr -> par
par: fix autocluster clang warning that is a bug
mpl2: fix clang warnings and old style iteration
Add comments for arguments of unconstraint endpoint func call.
regression subdir update
rm json.tcl
drt: handle dangling net in gr
drt: fix erroneous message in gr
pndgen -> pdn
TritonCTS -> cts
icewall ok
rsz remove hold buffer info
flow metrics
update flow metric limits
gui: in SelectHighlightWindow set the tab based on the current action (select/highlight)
requested PR changes + update sky130hd/ibex max slew viols
par: reset partitioners to off by default
mpl2: add unit test mp_test1
mpl2: update opendb
mpl2: remove debug output
grt: fix uninitialized variable
gui: removing leftover updateEnabled toggles
gui: forward declare Tcl_Interp in gui.cpp
remove deprecated grt option
remove deprecated tap option
metrics limits
gui: speedup logger output and add colors to error messages
gui: remove extra tcl.h includes
update metrics
flow sky130hd no endcap
remove -endcap_master from sky130* vars
grt: fix GlobalRouter vars and class members names
update ispd18_test3 drv limit
grt: fix Grid vars and class members names
grt: fix MakeWireParasitics vars and class members names
grt: fix more vars and class members names
grt: fix AntennaRepair vars and class members names
remove debug info
grt: fix uninitialized class member
Remove exit
ifp
ifp ceiling core space instead of floor
update regression_tests.tcl
replace template iterator in blif parser
unit test updates
removed exit in tests & added ok files
added missing eol
fix connected net destruction bug
rcx rm pointless dbu print
save_defok rm .defok existance check
pdr init for coverity
on-the-fly ap creation change
EOLEXTSPACING destroy malformed

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

    [Sync Up] change utEden CMake lib dependency to fit new Eden modules
    [unittest] temporarily disabled ut of cts cuz of structure changes

### gtest

find unit test log here:
[ut.note](./ut.note)

    [----------] Global test environment tear-down
    [==========] 31 tests from 27 test suites ran. (38 ms total)
    [  PASSED  ] 31 tests.

All test passed

### regression

find regression log here:
[regression.note](./regression.note)

    Passed all tool regressions.