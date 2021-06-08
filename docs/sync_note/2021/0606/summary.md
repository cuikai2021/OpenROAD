# Sync Up note 20210606

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

    1. Add regression of skew metric
    which require package json for tcl[0]
    2. function optimization for ppl, tap, drt, gui and work flow
    3. debug for ifp, rsz, utl and work flow
    4. update documents

[0] The new regression has new dependency package `json.tcl` which is included in `tcllib`
The original project `openRoad` has update script `etc/DependencyInstaller.sh` to install `tcllib` with `apt`.
After test in BX's PC, the regression is able to work after installation of `tcllib`.
If `regression` is still not able to work after installing `tcllib`,
Try to download the binary here <http://www.tcl-lang.org/software/tcllib/>
Then unzip the repository and try install all package with following cmd:

    cd tcllib-xxx
    ./configure
    sudo tclsh installer.tcl

## Detail Updates

    *install tcllib
    *fix DependencyInstaller.sh default option

    flow test clock skew limits
    flow clk skew metric
    flow clk skew metrics
    flow require json
    local json.tcl until tcllib is installed
    flow check metrics against limits
    fixed out-of-die viols with thick wires

    ppl: add comment for sortConstraints
    ppl: restore createSectionsPerConstraint function
    ppl: sort only overlapping constraints
    ppl: sort constraints by pins per slots
    ppl: ok files for add_constraint8
    ppl: update num_slots for constraint sections
    ppl: new unit test
    ppl: move sections initialization to initConstraints
    ppl: add sections to Constraint struct
    ppl: modifying obstruction box for top layer code to make code mode legible
    ppl: adding keepout parameter to define_pin_shape so pins honor existing routing

    tap: false -> 0
    tap: cover more cases in check_symmetry
    tap: update check_symmetry function
    tap: remove wrong code
    tap: unit test for symmetry check
    tap: refactor cut_rows function

    ifp: fix message code
    ifp: remove unnecessary message
    ifp: move check
    ifp: unit tests for placement blockages
    ifp: cut rows around placement blockages

    rsz use debugCheck instead of #if
    rsz okfiles
    rsz debug retime->repair_setup
    rsz repair hold do not make setup worse

    utl metrics rm tool arg

    [DRT] Remove param file from flow test
    [DRT] Remove need for TritonRoute param file (#665)
    drt: clang-format src/dr
    drt: minor cleanups

    gui: ensure layout does not attempt to paint while design is still loading
    gui: fix fit of design with zero bounds

    rcx do not write .totCap file

    etc: add hash check for downloaded files
    etc: fix missing export
    etc: fix boost install

    flow scriptC
    docs correction: 2 billion units = 2 million microns
    restore ibex_sky130hs, jpeg_sky130hs w/o rcx (seg fault)
    ibex sky130hd hold slack
    requested changes for PR
    refactoring for PR
    Update index.rst
    Remove deprecated LEF merging from test
    switching to marking slots as blocked instead of removing altogether, and fixing getValidSlots to account for top layer as well

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

    no conflicts

#### fix bugs after merge

    no bugs need to be fixed

### gtest

find unit test log here:
[ut.note](./ut.note)

    [----------] Global test environment tear-down
    [==========] 48 tests from 34 test suites ran. (2170 ms total)
    [  PASSED  ] 48 tests.

All test passed

### regression

find regression log here:
[regression.note](./regression.note)

    Passed all tool regressions.
