# Sync Up note 20210510

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

    1. Update README.md
    2. rename some part into abbreviation
    3. update Jednkins files and fixed them
    4. update src/CMakeLists.txt， test/regression, change lib names of those part which changed name 
    5. add more unit test or regressions
    6. refactor and optimize exist codes
    7. new codes and new functions
    8. removed deadcodes

## Detail Updates

    Update README.md
    rename `antenna_checker` to `ant`
    rename `openRCX` to `rcx`
    rename `include/openroad` to `include/ord`
    rename `flute3` to `sst`
    update Jednkins files and fixed them
    update src/CMakeLists.txt, change lib names of those part which changed name 
    add lib `pdngen` as `pdn`
    update test/regression to fit new lib names
    removed deadcodes

    ppl: -origin -> -region
    add new command `parse_layer_name` in ppl and optimize regression test
    ppl/utl: add shuffle function and move it into utl(algorithm.h)
    ppl: remove deprecated random alternatives
    ppl: remove random modes
    ppl: refactor assignPinsToSections function
    ppl: place constrained pins into special sections and add sections for constraints that have pins
    ppl: assigned constrained pins first
    ppl: assign constrained groups to special sections
    ppl: remove constraints check for hpwl computation
    ppl: remove constraints arguments
    ppl: add define_pin_shape_pattern command and support top layer constraint
    ppl: start slots and section creation for top layer constraints
    ppl: add unit test top_layer
    ppl: remove addPattern function from Core class
    ppl: correctly placement of pins at the top layer
    ppl: avoid recompute slots for top layer
    ppl: verify region constraint for top layer pins
    ppl: add unit tests for top layer placement
    ppl: top layer slots as global attribute
    ppl: initialize top_grid_
    ppl: report layer name in top layer constraint
    ppl: support for * keyword for region and pin_names
    ppl: check and error message for empty pin list
    ppl: rename .t and .tcl files
            I_FILE    src/ioplacer.i
            SCRIPTS   src/ioPlacer.tcl
            I_FILE    src/IOPlacer.i
            SCRIPTS   src/IOPlacer.tcl
    ppl: update parse_pin_names
    ppl: add placePin function and place_pin command
    ppl: ignore FIRM and LOCKED pins
    ppl: add check for pin count on place_pin command
    ppl: refactor randomPlacement

    optimized rsz estimate_parasitics corner test
    rsz add function findFaninFanouts
    rsz achive set_layer_rc command with -corner argument
    rsz repair_design corner issues
    rsz findFaninFanout exclude register pins
    rsz cellWireDelay
    rsz do not buffer unconnected inputs/outputs
    rsz command set_layer_rc arg check
    rsz repeater on input port

    grt: add extra pin access guide for rect only layers
    grt: add extra pin access guide for pins in routing layer 1
    grt: initial support for NDRs
    grt: remove deprecated clock_tracks_cost option
    grt: rename ndr unit test
    add ndr commands (such as init ndr)
    achive command `assign_ndr -all_clocks`
    fix ndr defout
    grt: add isUnidirectional function
    grt: return congestion information
    grt: update resources analysis information
    grt: add max h and max v overflow in congestion report

    TritonRoute: add spacing table influence functions
    TritonRoute: rewrite functions in src/gc/FlexGC_impl.h check90RectsMetSpcTblInf() 
    TritonRoute: added initRouteShapeCost as a parameter for TR
    TritonRoute: Grid graph cost names refactored: drc cost is now route shape cost;shape cost is now fixed shape cost
    TritonRoute: drt: log net/pin/ap info in debug
    TritonRoute: drt: take bottomRoutingLayer/topRoutingLayer by name to match the flow
    TritonRoute: db modInfSpacingRulesCost
    TritonRoute: db modInfSpacingRuleOrthBox
    TritonRoute: dr: activate modInf
    TritonRoute: drt: remove dead code
        Remove searchRepair useless options: markerBloat, enableDRC, fixMode
        and all associated code.  These options are always passed the same
        alues and the need to change them is gone; there were for older experiments.initMazeCost_marker(),mazeIterInit(), mazeIterInit_searchRepair()
    TritonRoute: drt: updates for unidirectional routing
        Treat layers with a RECTONLY constraint or more than one mask as unidirectional.
    TritonRoute: drt: partial retreat on USENONPREFTRACKS
        Semi-revert of 71251f3 which allows gf12 to work but also keeps the
        options that enable intel22 as well.  Hopefully this can be reverted
        again later.
    TritonRoute: drt: cross-thread qt signals are not working so single thread dr gui

    dpl: adding prefix control to filler placement
    dpl: obstruction support
    dpl: check_placement with obstructions
    
    switching names of max thread functions to thread count to be conistent with sta naming
    adding info message indicating thread count
    ensuring thread print statement only prints if different from default  on startup and updating readme
    adding set_thread_count and thread_count to tcl commands
    adding additional checking for thread count
    swig: add write_macro_lef cmd
    update all_clocks

    pdn: initial addition of c++ wrapper for global connections
    pdn: adding global connect list and license statements
    pdn: updating tmep sensor test to correct for different order of VDD/VIN connections
    removed opensta dependence from pdn
    requested changes to pdngen, fixing forloops and early return if insts is empty
    pdn: correcting include paths for tla change

    gui: include master name in path details
    gui: ensuring settings are saved when openroad exits via 'exit' from tcl
    gui: handle non-zero ORIGIN by using placement boundary

    odb: set MASK from LEF on the dbTechLayer
    odb: fix handling of non-zero ORIGIN in LEF
    odb: removing many dead classes and trim unused methods/members
    odb/rcx: remove dead code

    db_sta isSpeciall(net)
    sta report_power ibex/sky130hd seg fault
    dbSta: add sta::get_all_clk_nets tcl api


    rcx: Clean up commented code and add describtion on couple function
    rcx: fix name mapping problems

    tap: remove redundant check (-endcap_master) and set it as "NULL"
    tap: correcting incorrect utl::warn call in tapcel

    ignore mimcap layers
    optimize gcd sky130 fast/slow corner flow test
    update boost url

## debug information

### build

find build log here:
[logBuild.note](./logBuild.note)

After fixing bugs of UT an CMakeLists.txt (didn't touch any source code)
Eden can be built successfully.

### git

find git log here:
[git.note](./git.note)

#### Conflicts

    both modified:   src/TritonCTS/src/LevelBalancer.h          <105~106 accept both>
    both modified:   src/ppl/include/ppl/IOPlacer.h             <153~160 accept both> <234~237 accept both>
    both modified:   src/ppl/src/IOPlacer.tcl                   <243~318 accept current>
    both modified:   src/ppl/test/regression_tests.tcl          <34~44 accept both>
    both modified:   src/tap/src/tapcell.tcl                    <1004 accept current>

#### fix bugs after merge

    src/CMakeLists.txt                      sync lib name change in utEden target_link_libraries
    [UT] EdenTestTritonCTS_CLOCK.cc         remove test for changed function SubNet::remove()
    [UT] EdenTestANTENNA_CHECKER.cc         change headers path to fit new name for antenna cheker(ant)
    .github                                 remove

### gtest

find unit test log here:
[ut.note](./ut.note)

    [----------] Global test environment tear-down
    [==========] 32 tests from 27 test suites ran. (0 ms total)
    [  PASSED  ] 32 tests.

All test passed

### regression

find regression log here:
[regression.note](./regression.note)

    Found 1 tool regression failures.

    tap
    cut_rows pass
    gcd_fakeram pass
    gcd_nangate45 pass
    gcd_sky130 pass
    invalid_cells pass
    multiple_calls pass
    avoid_overlap pass
    boundary_macros pass
    gcd_prefix pass
    gcd_ripup pass
    no_endcap *FAIL*
    ------------------------------------------------------
    Failed 1/11
    See /home/zekungao/work/SyncUp/Eden/src/tap/test/results/diffs for differences
    See /home/zekungao/work/SyncUp/Eden/src/tap/test/results for log files

This sync up added a new test `no_endcap` for `tap`

Our tool didn't pass it.
