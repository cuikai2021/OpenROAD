# Sync Up note 20220218

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

## Detail Updates

1. updated `DependencyInstaller`, **need to update boost version**
2. add new modules `dpo` and `dst`
3. resolved conflicts in `GenPdn.tcl`
4. some regression ok files has been updated in `mpl`, there is an error in original tcl script of `gcd_mem5_01`
5. ord has cleaned up codes and use the clang formatter

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

	both modified:   .gitmodules							Add new submodule "abc"
	both modified:   CMakeLists.txt							Add `if(NOT EDEN_VERSION)` to decide whether show the git version message
	both modified:   etc/DependencyInstaller.sh				Add arguments in `./b2.sh install` of boost
	both modified:   src/CMakeLists.txt						Add new module `dst` and rename `ICeWall` as `pad`
	both modified:   src/Main.cc							Add a handler to trace stack
	both modified:   src/cts/src/LevelBalancer.cpp			Formatting of function arguments
	both modified:   src/dbSta/include/db_sta/dbSta.hh		add new member variables
	both modified:   src/dpl/src/Place.cpp					Try to keep TD's updates
	both modified:   src/dpl/test/max_disp1.ok				update ok file
	both modified:   src/pdn/src/PdnGen.tcl					accept incoming changes

#### fix bugs after merge

	update regression ok files
	fix unfound blockages failure in testcase pdn/four_sides
	fix unit test failure
	update ok file of mpl, use old regression tcl script for gcd_mem5_01

### gtest

find unit test log here:
[ut.note](./ut.note)

All test passed

### regression

find regression log here:
[regression.note](./regression.note)
