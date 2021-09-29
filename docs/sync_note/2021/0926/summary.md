# Sync Up note 20210926

Sync up from ICBench/openRoad

url:        <https://github.com/ICBench/OpenROAD.git>
branch:     master

## Summary

1. changed version method
    use `EDEN_GIT_DESCRIBE` to replace `EDEN_GIT_SHA1`
    (updated docker files to adapt this change)
2. drt
    refactor structures
    use enumerations defined in openDB directly
    remove redundant values
3. updated gui
4. updated PdnGen.tcl
    some changes about straps
    (which caused some bugs in our local testcases, now fixed)
5. refactor tapcell
    rewrite codes directly into cpp
    clean up tcl codes and remove redundant contents
    some optimizations
6. stt
    new test cases and clean up codes

## Detail Updates

rm space
revert to scaped dquotes
Fix list Python classes
do not show version on prompt
random whitespace and tab rm
Use git describe as the version
Update odb python interface docs
gui: ruler, adding missing header
gui: correcting units on capacitance for layer descriptor
gui: removing isRepaint code, remapping redraw to full repaint, and adding shortcut for renderers to redraw
Revert "many: make renderers in other tools always require repainting"
[DRT] Simplify odb aliases
[DRT] Replace frTermDirectionEnum with dbIoType
[DRT] Replace frTermEnum with dbSigType
[DRT] Replace frNetEnum with dbSigType
[DRT] Replace frOrient with dbOrientType
[DRT] Replace MacroClassEnum with dbMasterType
[DRT] Replace frLayerTypeEnum with dbTechLayerType
[DRT] Replace frPrefRoutingDir with odb::dbTechLayerDir
[DRT] Change zDirs_ vector to enum for clarity
[DRT] Convert getPrevAstarNodeDir to use FlexMazeIdx
[DRT] cleanup unused code
odb: include sstream for mac build error
gui: adding basic tech layer descriptor and adding signal to select layer from display controls
gui: switch unicode of mu to variable for readablility
gui: add tooltip for routing layers with direction, min width/spacing, and resistance and capacitance if available
par: some coverity issues addressed
gui: fix coverity issues
odb: add -masters to write_cdl
Fix coverity issue and wrong indexing for DELAY_4 enum value
Added debug lines
Take note of WRONGDIRECTION widthtable rules, and apply them when determining the sizes of vias being created
many: make renderers in other tools always require repainting
gui: fix sta and drc renderers to correctly request repaints
gui: correct instance name drawing
gui: adding cache to layout viewer
gui: reverts 016f37dcc28a76752c036ad96289a13ac10a6bc4 + gives RulerDescriptor copy of rulers vector
gui: ruler manager singleton + renable editing on name
Reduce the width of vias on intermediate layers in the non-preferred routing direction, allowing more routing space. Correct the calculation for the number of via rows to ensure that the via is contained within the overlapping metal shapes
Rename status variable to success
gui: ensure ruler names are unique and prevent modifying ruler name
gui: switch from using bool& to return pair instead
gui: fix ANCHOR -> Anchor
gui: addressing comments about const and d1/dl
gui: fix copyright
gui: remove left over code
remove unnecessary comments
Update tap when there is no tapcell needed
handle git describe failure when generating version string
gui: snap to end points and center, fix drawing of ruler ticks for small rulers, adding shift to allow of different snapping
gui: fix font hack in drawString and remove need to track unique transforms
gui: fixing ruler intersection check to handle angled rulers better
gui: make single function to handle next ruler point
gui: adjust drawing of rulers to account for angled rulers and text
gui: make rulers controllable from display controls
gui: basic snapping to metal shapes + point and click rulers
gui: changing drawString to use anchor
gui: adding label to drawRulers
gui: initial conversion to rulers as classes with descriptor
Add is_area_mode_
Correctly check spdlog installed in Ubuntu
Add note about compiler in PATH
Check before installing
Cleanup dependencies
update flow.tcl for version change
Change version string from sha1 to 'git describe' output
gui: partial revert of 27281826ea6e78d0c73def81caa893942c23fd27
Add if statement to skip tapcell insertion
Fix regression
GR-Pin fix
gui: update comments
gpl: add incremental placement for unplace instances
gui: fix layout viewer size when viewport changes
Remove code related to define outside ifdef
Remove RCX hard-coded define for debug net.
gui: give obstructions and blockages basic names in descriptors
revert getRouteBox change in FlexDR_graphics
fixes #1036
fix
gui: switch addToOutput to rely on outputChanged, which calls repaint instead of processes events, to prevent recursive paint calls
gui: switch redraw to use update as it ensures recursive calls are handled properly
gui: adding comments to the layout viewer
gui: standardize some of the highlighting commands in gui
tap: minor fixes
tap: fixed CI tests
bug fix
tap: fixed type name, moved some functions for swig utilization
show gui after no path error
tap: fixed comments, type names
clang-format
create graph edges for unreachable on-the-fly aps
tap:re-ordered boundary_macros.defok, added comments, minor fixes
gui: fixing segfault in timing widget when switching models
stt okfiles
stt checkTree ignore zero length branches
gui: correct behavior of DRC widget to account for keyboard selection changes
gui: correct centering behavior when zooming in and out
A stripe on a rails layer will be horizontal, regardless of the preferred direction setting in the tech file
gui: correct behavior of fit_pixels_per_dbu_ to ensure it is updated when window changes
gui: ensure 0 and scale bar size do not overlap and avoid drawing ticks when they are spaced closely together
gui: change scale bar view
gui: ensure entire view is redrawn when scrolling through the layout
gui: center scale bar text over bar
gui: adding picometer scale
tap: optimized and adjusted loops, variables, function returns and data structures along the code
Improve fixing of p/g in channels, ensure full width connections to p/g nets are present inside channels and that channel shapes identified are rectangles rather than polygons.
gui: add scale bar to layout view
stt okfiles
check.tcl defeat stupid keyword checker failure in comment
pd cleanup
stt check pd result and fall back to flute
gui: add countdown to pause button
gui: adding timeout to pause
fix null pointer + * in comment
remove printing
gui: adding line number to DRC violation
gui: adding combined file open filter
gui: ensuring inspector columns are resized on load
Revert "gui: coverity rand() is not used for crypto, no security concerns"
gui: add help action to gui to open help URL
gui: add main window icon
gui: coverity rand() is not used for crypto, no security concerns
gui: no point in drawing timing stage if there is no destination
gui: ensure ScriptWidget variables are initialized
gui: ensure HighlightGroupDelegate variables are initialized
gui: switch to use db_ in selectAt
gui: inspector coverty fix Matt identified
stt compile error
stt rm checkTree
gui: ensure TimingPathRenderer variables are initialized on startup
gui: ensure path_ is initialized in DetailedModel
- pre commmit hook dummy
- clang-format
Replace -grid_over_pins with grid_over_pg_pins and make this behavior the default. Add option -grid_over_boundary to get previous behavior. Show which option is being used in the log file
stt okfiles
stt okfiles
stt pd drvr_index moved by duplicate removal
stt const
closes #686
gui: inspector + dbDescriptor coverty fixes
rename TritonRoute to drt
gui: add unordered_map to mainWindow header and correct keyPressEvent in timingWidget.h
gui: clang-format drcWidget.cpp
gui: convert drc to use new report
drt: modify DRC report to include identifier for sources and convert two cout to logger
gui: clears timing path if its deselected
gui: inspector grab focus when selected
gui: correct column behavior in DRC viewer
gui: switch DRC violation to use odb geoms
Update README.md
gui: minor formatting
gui: adding error and warns to TritonRoute parsing
gui: only adding non-found sources to comment list
gui: correct horizontal labels
gui: provide feedback to DRC to mark violations as viewed
grt okfiles yet again
gui: remove sorting flag from detailed path in timing report
gui: make use of polygon drawing
gui: adding draw polygon to Painter
gui: adding some zoom margin for the DRC
gui: switch name of drc violation to layer and sources for TR report violations
gui: drc viewer automatically render when visible
stt check tree ignore zero rects
gui: convert render registration to be based on widget visibility, connect signals for sorting
gui: change name from timing_dialog_ to timing_widget_
gui: modify timing model to allow for sorting across all data
stt rearrange
pdrev mv translateTree inside pd/pdrev functions
Misc warns for rtd
Review
gui: basic interface for viewing DRC
gui: convert Renderer select to return set of selctions
gui: adding option for hashed brush in painter
gui: make options and pixels_per_dbu available from Painter
gui: provide access function in tcl to show/hide widgets in main window
gui: dockify timing report
Fix rebase issues
Add note about clang-format
Style
Missing word
Min requirements to compile and warn about docker
Add back mentions to restricted words.
Fix links to work in GitHub
Add how to check for broken links
Use build not _build for rtd
Use same style for all code blocks
Integration of ORFS documentation
Re-wording
Refactor documentation
Clean up: remove out-of-date files
rst -> md
drt: one less DRV on ispd19_test10 with ACCESS_PATTERN_END_ITERATION_NUM=10
tap: updated tcl code to c++ code
stt reportSteinerTree path depth for flute trees
grt: refactor isNonLeafClock
grt: fix isNonLeafClock
grt: use enum
grt: refactor getEdgeCapacity
grt: refactor clock nets detection
grt: clang-format
grt: update draw_route_guides command
drt: bump ACCESS_PATTERN_END_ITERATION_NUM to 10 for difficult AOI gate
drt: replace FlexPA::macroCellPinNoApCnt_ with ACCESS_PATTERN_END_ITERATION_NUM
gui: make row color part of display controls
gui: only draw instance obstructions if routing obstructions are visible
gui: ensure dbInst properties and editors are using same values
grt: use set instead of vector
grt: update command names
grt: restore const
pdr mv RemoveSTNodes from graph to PdRev
pdr segregate pdrevII code
drt: fix duplicate message id
drt: add debug GUI for pin access' combining step
pdr rm unused code
pdr cleanup
makeSteinerTree(alpha) for regressions
grt: fix min/max layers
grt: compute actual capacities for nets with layer restriction
add function to compute edge capacity considering layer restrictions
Revert "Merge pull request #864 from eder-matheus/grt_revert"
grt: add clear_route command
grt: add support for wildcars in highlight_route command
grt: add highlight_route as a public command
update Jenkinsfile.nightly for replace->gpl
gui: adding center and resolution access to tcl + correct behavior when zooming in/out
gui: make ruler precision dependent on DBU
gui: disable tcl input when paused
gui: adding tcl access for pause
Add error message when power/ground nets do not have global connections defined
Remove commented code
Modify scripts to support required time, use only delay4 for timing mode
Support multiple calls in same session
grt: refactorings for maze routeres
rename opendb->odb
spacingtable-opt squash

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

    etc/DependencyInstaller 
		<line 35~108, add $SUDO and $PROXYCHAINS, accept incomming> 
		<line 208~300, accept current>
	CMakeLists.txt 
		<line 69, accept incomming> 
		<line 64~71, OPENROAD=>EDEN, accept incomming>
	include/ord/Version.hh.cmake
		<line 3, OPENROAD=>EDEN, accept incomming>
	src/CMakeLists.txt
		<line 137~138, add Banyan, accept incomming>
		<line 183~184, add Banyan, accept incomming>
	src/Main.cc
		<line 180, OPENROAD=>EDEN, accept incomming>
		<line 252, accept incomming>
		<line 376~379, OPENRAOD=>EDEN,accept incomming>
	src/OpenRoad-py.i
		<line 49, openroad=>eden, accept incomming>
		<line 69, openroad=>eden, accept incomming>
	src/OpenRoad.i
		<line 282~285, openroad=>eden, accept incomming>
	src/tap/src/tapcell.tcl
		<accept all current, backup their tapcell.tcl as tapcell.tcl.bk>

#### fix bugs after merge

    [SyncUp] change warning number, add 200 to avoid log conflicts
	[SyncUp] typo mistake OPENROAD_GIT_DESCRIBE to EDEN_GIT_DESCRIBE
	[SyncUp] update prof.WJ's modification to adpat new enum value
	[SyncUp] update utest dependency to adapt drt rename
	[SyncUp] use openroad tapcell.tcl to avoid tcl crash
	Merge remote-tracking branch 'upstream/devops-dev' into tmp
	Merge remote-tracking branch 'upstream/master' into tmp
	[SyncUp] patch to relove failed testcase icbmode
	Merge remote-tracking branch 'upstream/master' to disable icbmode regressions
	remove unused `include/openroad/Version.hh`
	[SyncUp] use EDEN_GIT_DESCRIBE to replace EDEN_GIT_SHA1 incase cannot show eden version
### gtest

find unit test log here:
[ut.note](./ut.note)

    [----------] Global test environment tear-down
    [==========] 31 tests from 27 test suites ran. (35 ms total)
    [  PASSED  ] 31 tests.

All test passed

### regression

    Passed all tool regressions.

find regression log here:
[regression.note](./regression.note)
