# Sync Up note 20210516

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

    1. Update README.md
    2. optimize grt to support 3D maze
    3. clean up codes
    4. ppl: rename some functions
    5. debug in drt
    6. par: use option (random_seed) to change random seed

## Detail Updates

    Update README.md
    Rework the automatically sync branch github action.
    resetting default seed to reset on each call to partition_netlist and removing extra function
    ensuring random seed is the same from openroad session to session

    grt: fix edgeCostsPerLayer assignment
    grt: use per-layer tracks consumption in maze3D
    grt: fix overflow check
    grt: update ndr tests
    grt: allow 3D overflow for ndrs
    grt: use per layer track consumption
    grt: add per layer track consumption for ndrs
    Revert "grt: remove *100 for congest driven place api"

    [DRT] Fix Y --> X typo in getFrOrient

    ppl: switching parse_pin_names to use database instance conversion instead of using names to find bterms
    ppl: clear_constraints -> clear_io_pin_constraints
    ppl: correct typo in warn 44 message referencing missing variable

    par: ensuring random seeds change from run to run with option (random_seed) to keep seeds the same
    par: switching std::vectors to const to conform with swig interface template

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
    [==========] 32 tests from 27 test suites ran. (1 ms total)
    [  PASSED  ] 32 tests.

All test passed

### regression

find regression log here:
[regression.note](./regression.note)

    Passed all tool regressions.
