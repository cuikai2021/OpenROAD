# Sync Up note 20210704

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

    1. simplify and optimize logger messages(format)
    2. update regression to fit changes of messages and add new regressions
    3. drt: skip graph enablement on special nets
        Modified arguments of tcl cmd `global_route`
            `-overflow_iterations` => `-congestion_iterations`
        (relative functions name changed also)
        This might cause the tape out flow failure
        plz checkout relative changes in openRoad-flow-script
    4. gpl: ensure minimum number of bins is 2x2
    5. ppl: add flag to allow min_distance in number of tracks instead of microns
    6. added new arguments in tcl cmd
        `place_pins`: `-min_distance_in_tracks`
        `repair_design`: `-max_slew_margin`, `-max_cap_margin`
        (and their relative functions)
        check more details in `README.md`

## Detail Updates

    gui: correcting highlighting for else, elseif, and commands within {}
    update ok files with fixed messages
    update ok files with fixed messages
    fix ant messages
    rsz sideline buffer_ports6
    ppl: unit tests
    ppl: update readme
    ppl: add flag to allow min_distance in number of tracks instead of microns
    rsz rm debug
    ibex_sky130hs max cap
    rsz flow metrics
    rsz okfiles
    rsz remove special net exceptions
    comment
    n45, sky130 updated layer rc
    update layer rc
    Create CODE_OF_CONDUCT.md
    drt: report all constraints properly in the drc report
    pad: correct variable referenced in error PAD-15
    Apply review comments
    rsz repair_design margins use values instead of percentage
    rsz metrics
    Updates to allow bp_single testcase to run through ICeWall
    grt: bug fix
    grt: overflow_iterations -> congestion_iterations
    repair_design -max_slew_margin -max_cap_margin
    rsz repair_hold oks
    drt: skip graph enablement on special nets
    gpl: ensure minimum number of bins is 2x2
    gpl: remove messages.txt
    rsz resize hold buffers
    rsz repair_design on input ports
    nangate45/sky130hd/sky130hs set_layer_rc to match rcx
    sky130hd adj layer caps

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

    both modified:   src/pdngen/src/PdnGen.tcl   <line 3553, accept incoming change, The two modifications are the same>

##### fix bugs after merge

    [unittest] modify ok files to fit new logger format

### gtest

find unit test log here:
[ut.note](./ut.note)

    [----------] Global test environment tear-down
    [==========] 55 tests from 37 test suites ran. (4391 ms total)
    [  PASSED  ] 55 tests.

All test passed

### regression

find regression log here:
[regression.note](./regression.note)

    Passed all tool regressions.
