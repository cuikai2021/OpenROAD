# Sync Up note 20210627

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

    1. add all message.txt into .gitignore
    2. update metrics and apply it into regression
        it allow us to generate json report
        which makes benchmark easier
    3. update Licence ,README.md and regressions
    4. add docs for ICeWall and update its regression scripts
    5. grt(global routing): rename functions, 
        add functions to achieve random grt
        apply new grt in rsz

## Detail Updates

    grt: update readme
    grt: minor fixes
    fix wrong nb_ pointer inside rb_ strucutre
    thrashing
    thrash
    update flow metrics
    ibex_sky130hs max cap
    grt: fix readme
    grt: minor fixes
    grt: remove random tests
    grt: add set_global_routing_random description
    flow_metrics.tcl return multiple failures
    ibex_sky130hd max cap
    grt: avoid pass local nets to fastroute core
    rsz use alpha 0.3 instead of set_routing_alpha for now
    ibex_sky130 slew violations
    rsz use alpha 0.3 instead of set_routing_alpha for now
    flow_metrics.tcl require json for some procs
    The help output changed
    flow_metrics.tcl require json for some procs
    jpeg sky130hd metric limits
    grt: return error in test scripts
    grt: fix message
    grt: factor nets[newnetID] to a local variable
    grt: remove check_pdrev_alpha proc
    ibex_sky130hd.metrics_limits
    rsz repair_setup iteration limit
    update flow metrics
    ibex max cap limit
    grt: _netsAlpha -> _nets_alpha
    grt: set_pdrev_alpha -> set_routing_alpha
    tweaks
    tweaks
    update figures
    rsz init grt accessor update
    rm text from figures and add legends
    rsz loosen flow metrics
    rsz journal restore issue
    gpl td test okfile
    grt: update readme
    grt: getFastRoute -> getGlobalRouter
    grt: add unit tests using pd
    grt: use pd instead of pdrev
    grt: check net alpha to enable pdrev
    grt msg conflict
    grt: refactor nets[i]
    grt: pass root index to pdrev instead of swap
    grt: correct check for alpha value + unit tests
    grt: rm set_clock_routing
    grt: set_pdrev_alpha_for_net -> set_pdrev_alpha
    grt: remove unnecessary check
    grt: use reference for root pin idx
    grt: Pin::isDriver()
    grt: fixes for pdrev vars and command names
    grt: remove message
    grt: add debugCheck
    grt: report -> debugPrint
    grt: minor fixes
    grt: remove unused var
    grt: fix message
    grt: avoid pass local nets to fastroute core
    grt: fix comment
    grt: fastRoute_ -> global_router_
    grt: add comment
    grt: add warn message when insert diode hit max iters
    rsz set_routing_alpha, use pd
    Revert "- remove truncatedStyle checks" due to secure-ci failures
    grt: allow dpl to move illegal placed diodes
    grt: add iterations limit to legally place a diode
    grt: check current capacities before restoring old capacities
    grt: remove default value from addAdjustment function
    grt: add test for repair_antennas with iterations
    grt: bug fix for diodes inserted near macros
    grt: fix -iteration option for repair_antennas
    grt: add unit tests for repair_antennas
    grt: fix comment
    grt: check diode in tcl
    grt: update readme
    grt: allow dpl modifies diodes
    grt: update repair antenans loop with correct diode names and iteration report
    grt: add iterations option for repair_antennas command
    grt: get macro pin box instead of macro box
    reduce warnings
    grt: remove unnecessary limit
    grt: remove wronge delete[]
    grt: add limit for pins out of die warns
    update flow.tcl
    grt: error out when detect invalid tree
    update ant regression test
    run-ispd.tcl fix
    Update GettingStarted.rst
    remove internal cmds
    various updates / PR review
    FlexDR_graphics::show() considering -net flag
    gui: address coverity
    Use bump centre rather than origin for location of pin shape, ensure same bump is not used twice, code fixes
    Delete messages.txt
    dbSta/Sta::findLogicConstants
    path seg merge fix
    run-ispd.tcl updates: drv limits + show run.log
    Correct copyright notices
    Update index.rst
    .gitignore messages.txt
    Remove the messages.txt from git as they are now auto-generated
    grt: ok files

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

    both modified:   .gitignore                     <line 27~32 accept both>
    deleted by them: src/ICeWall/messages.txt       <delete>
    both modified:   src/ICeWall/src/ICeWall.tcl    <line 214~234 accept incoming, line 507~545 accept both, line 565~607 accept both>
    deleted by them: src/TritonRoute/messages.txt   <delete>
    deleted by them: src/ppl/messages.txt           <delete>
    deleted by them: src/tap/messages.txt           <delete>

#### fix bugs after merge

    [SyncUp] resolve duplicate error number 100 in ICeWall.tcl

### gtest

find unit test log here:
[ut.note](./ut.note)

    [----------] Global test environment tear-down
    [==========] 55 tests from 37 test suites ran. (4020 ms total)
    [  PASSED  ] 55 tests.

All test passed

### regression

find regression log here:
[regression.note](./regression.note)

    pdngen
    test_gcd pass
    test_gcd_dual_rails pass
    test_gcd.invert_viarule pass
    test_gcd_M7_pin pass
    fail_quick pass
    tempSensor pass
    tinyRocket pass
    soc_bsg_black_parrot_nangate45 *FAIL*
    soc_bsg_black_parrot_nangate45.pad_offset *FAIL*
    extend_to_core_ring pass
    asap7_vias pass
    asap7_vias.noviarules pass
    ------------------------------------------------------
    Failed 2/12

    Found 1 tool regression failures.
