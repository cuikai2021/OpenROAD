# Sync Up note 20211110

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

1. Updated `openroad` exe flags
    Add `-max` (same as `-theads cout`) `-gui` `-python` `-log`
    **now gui mode is available**
2. Updated doc generating scripts and README.md
3. Updated DependencyInstaller
    apt-get install `qt5-image-formats-plugins`
    to support gui mode
4. Updated ICeWall
    New tcl functions to set pad cell center and get pin informations
    Refactor `get_tech_top_routing_layer` function
    Added new regression testcases
5. cts
    refactor setting clock buffer master
6. dbSta
    achieve function `makeTopPort`
    use DbModule to replace DbInsts
7. gui
    Enable gui mode.
    Now it is able to run openroad with a gui (`openroad -gui`)
    Functions are not completed
8. dpl
    add a new feature `Grapics.cpp`
9. drt
    rename relative class `fr*`=>`*`
    refactor object structure and file architecture
10. refactor par (almost totally rewrite)

## Detail Updates

Check commits update

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

    both modified:   src/drt/src/io/io.cpp <line 2614, type name changed, accpet incoming>

#### fix bugs after merge

    [SyncUp] frPoint => Point to adapt type name change

### gtest

find unit test log here:
[ut.note](./ut.note)

All test passed

### regression

find regression log here:
[regression.note](./regression.note)

    Passed all tool regressions.
