# Copyright (c) 2019, Parallax Software, Inc.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/src/cmake")

include("openroad")

set(OPENSTA_HOME ${PROJECT_SOURCE_DIR}/src/sta)
set(OPENDB_HOME ${PROJECT_SOURCE_DIR}/src/OpenDB)
set(DBSTA_HOME ${PROJECT_SOURCE_DIR}/src/dbSta)
set(RESIZER_HOME ${PROJECT_SOURCE_DIR}/src/rsz)
set(PDNGEN_HOME ${OPENROAD_HOME}/src/pdngen)
set(ICEWALL_HOME ${OPENROAD_HOME}/src/ICeWall)

set(OPENROAD_SOURCE
  OpenRoad.cc
  Main.cc
  )

################################################################

# OSX tcl is deprecated and prints a warning, so look for a user installed
# version before using the system version.
# I tried to override the library search order instead but failed.
# CMAKE_FIND_FRAMEWORK LAST bypasses the version in the framework directory
# but not the one in /usr/lib.
# This calls cmake/FindTCL.cmake
# Do not use REQUIRED because it also requires TK.
find_package(TCL)

# check for tclReadline
set(TCL_READLINE_POSSIBLE_NAMES tclreadline-2.1.0
    tclreadline-2.3.2 tclreadline-2.3.6 tclreadline-2.3.7 tclreadline-2.3.8
    )
find_library(TCL_READLINE_LIBRARY
  NAMES tclreadline ${TCL_READLINE_POSSIBLE_NAMES}
  PATHS ${TCL_LIB_PATHS}
  )
if (TCL_READLINE_LIBRARY)
  message(STATUS "TCL readline library: ${TCL_READLINE_LIBRARY}")
endif()

find_path(TCL_READLINE_H tclreadline.h)
if (TCL_READLINE_H)
  message(STATUS "TCL readline header: ${TCL_READLINE_H}")
endif()

option(BUILD_TCLX "Build with tclX included" ON)
if (BUILD_TCLX AND TCLX_LIBRARY)
  message(STATUS "TclX library: ${TCLX_LIBRARY}")
endif()
if (BUILD_TCLX AND TCLX_H)
  message(STATUS "TclX header: ${TCLX_H}")
endif()

find_package(SWIG 3.0 REQUIRED)
include(UseSWIG)

find_package(Boost REQUIRED)
find_package(Python3 COMPONENTS Development REQUIRED)
option(BUILD_PYTHON "Build the Python3 interface" ON)

################################################################

swig_lib(NAME      openroad_swig
         NAMESPACE ord
         I_FILE    OpenRoad.i
         SCRIPTS   ${OPENSTA_HOME}/tcl/Util.tcl
                   OpenRoad.tcl
                   ${ICEWALL_HOME}/src/ICeWall.tcl
                   ${PDNGEN_HOME}/src/PdnGen.tcl
                   ${OPENDB_HOME}/src/db/OpenDB.tcl
)

target_link_libraries(openroad_swig
  PRIVATE
    dbSta
    opendb
    OpenSTA
)

################################################################
#
# Library dependencies
#
################################################################

# Zlib
include(FindZLIB)
# Translate cmake bool to StaConfig.h ifdef bool
if (ZLIB_FOUND)
  set(ZLIB 1)
else()
  set(ZLIB 0)
endif()

find_package(spdlog REQUIRED)
message(STATUS "spdlog: ${spdlog_VERSION}")

find_package(Threads REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG ON)

# gtest and gmock
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIR})
message(STATUS "gtest path: ${GTEST_INCLUDE_DIR}")
# get_filename_component(GTEST_LIBRARIES_DIR ${GTEST_LIBRARIES} DIRECTORY)
message(STATUS "gtest lib: ${GTEST_LIBRARIES}")

# Enable gtest and gmock with pthread
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lgtest -lgmock")

################################################################

# Build flow tools
add_subdirectory(ifp)
add_subdirectory(OpenDB)
add_subdirectory(sta)
add_subdirectory(dbSta)
add_subdirectory(rsz)
add_subdirectory(flute3)
add_subdirectory(replace)
add_subdirectory(dpl)
add_subdirectory(fin)
add_subdirectory(ppl)
add_subdirectory(TritonCTS)
add_subdirectory(grt)
add_subdirectory(tap)
add_subdirectory(mpl)
add_subdirectory(OpenRCX)
add_subdirectory(psm)
add_subdirectory(PartitionMgr)
add_subdirectory(antenna_checker)
add_subdirectory(gui)
add_subdirectory(TritonRoute)
add_subdirectory(utl)
add_subdirectory(pdrev)
add_subdirectory(utt)

################################################################

add_executable(eden
  ${OPENROAD_SOURCE}
)

target_compile_options(eden
  PRIVATE
    -Wall -Wextra -pedantic -Wcast-qual -Wredundant-decls -Wformat-security
    $<$<CXX_COMPILER_ID:Clang>:-Wno-gnu-zero-variadic-macro-arguments>
    $<$<CXX_COMPILER_ID:AppleClang>:-Wno-gnu-zero-variadic-macro-arguments>
)

# Disable compiler specific extensions like gnu++11.
set_target_properties(eden PROPERTIES CXX_EXTENSIONS OFF)
target_compile_features(eden PUBLIC cxx_std_17)

target_include_directories(eden
  PUBLIC
    ../include
)

target_link_libraries(eden
  ifp
  openroad_swig
  replace
  dpl
  fin
  rsz
  ppl
  flute
  dbSta
  OpenSTA
  opendb
  opendbtcl
  OpenRCX
  TritonCTS
  grt
  tap
  gui
  TritonRoute
  mpl
  psm
  antennachecker
  PartitionMgr
  utl
  utt
  ${TCL_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT}
  ${GTEST_LIBRARIES}
)

# tclReadline
if (TCL_READLINE_LIBRARY AND TCL_READLINE_H)
  target_compile_definitions(eden PRIVATE ENABLE_READLINE)
  target_link_libraries(eden ${TCL_READLINE_LIBRARY})
  target_include_directories(eden PRIVATE ${TCL_READLINE_H})
  message(STATUS "TCL readline enabled")
else()
  message(STATUS "TCL readline disabled")
endif()

if (BUILD_TCLX AND TCLX_LIBRARY AND TCLX_H)
  target_compile_definitions(openroad PRIVATE ENABLE_TCLX)
  target_link_libraries(openroad ${TCLX_LIBRARY})
  target_include_directories(openroad PRIVATE ${TCLX_H})
  message(STATUS "Tcl Extended enabled")
else()
  message(STATUS "Tcl Extended disabled")
endif()

if (ZLIB_FOUND)
  target_link_libraries(eden ${ZLIB_LIBRARIES})
endif()

# Optional CUDD library for OpenSTA
if (CUDD_LIB)
  target_link_libraries(eden ${CUDD_LIB})
endif()

if (Python3_FOUND AND BUILD_PYTHON)
  message(STATUS "Python3 enabled")
  target_compile_definitions(eden PRIVATE ENABLE_PYTHON3)

  swig_lib(NAME      openroad_swig_py
		   NAMESPACE ord
		   LANGUAGE  python
		   I_FILE    OpenRoad-py.i
		   SCRIPTS   ${CMAKE_CURRENT_BINARY_DIR}/openroad_swig_py.py
  )

  target_link_libraries(openroad_swig_py
    PRIVATE
      dbSta
      opendb
      OpenSTA
  )
  target_link_libraries(eden
    openroad_swig_py
    opendbpy
  )
else()
  message(STATUS "Python3 disabled")
endif()

# Ancient versions of glibc found on CentOS required librt.
# OSX has an empty version of librt that causes a link error,
# so this is protected from linking there.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_library(LIBRT rt)
  target_link_libraries(eden ${LIBRT})
endif()

################################################################
# Install
# cmake .. -DCMAKE_INSTALL_PREFIX=<prefix_path>

# executable
install(TARGETS eden DESTINATION bin)

################################################################

add_custom_target(openroad_tags etags -o TAGS
  ${OPENROAD_SOURCE}
  ${OPENROAD_HOME}/include/openroad/*.hh
  ${OPENROAD_HOME}/src/dbSta/src/*.hh
  ${DBSTA_HOME}/include/db_sta/*.hh
  ${OPENROAD_HOME}/src/dbSta/src/*.cc
  ${RESIZER_HOME}/src/*.hh
  ${RESIZER_HOME}/src/*.cc
  WORKING_DIRECTORY ${OPENROAD_HOME}/src
  DEPENDS ${OPENROAD_SOURCE} ${OPENROAD_HEADERS} ${OPENROAD_TCL_FILES}
)

#########################
# build unit test

add_executable(unittest
../utest/utt/EdenTestUTT.cc
../utest/utl/EdenTestUTL.cc
../utest/TritonRoute/EdenTestTRITONROUTE.cc
../utest/TritonCTS/EdenTestTRITONCTS.cc
../utest/tap/EdenTestTAP.cc
../utest/sta/EdenTestSTA.cc
../utest/rsz/EdenTestRSZ.cc
../utest/replace/EdenTestREPLACE.cc
../utest/psm/EdenTestPSM.cc
../utest/pdrev/EdenTestPDREV.cc
../utest/PDNSim/EdenTestPDNSIM.cc
../utest/pdngen/EdenTestPDNGEN.cc
../utest/PartitionMgr/EdenTestPARTITIONMGR.cc
../utest/OpenRCX/EdenTestOPENRCX.cc
../utest/OpenDB/EdenTestOPENDB.cc
../utest/ifp/EdenTestIFP.cc
../utest/ICeWall/EdenTestICEWALL.cc
../utest/gui/EdenTestGUI.cc
../utest/grt/EdenTestGRT.cc
../utest/flute3/EdenTestFLUTE3.cc
../utest/fin/EdenTestFIN.cc
../utest/dpl/EdenTestDPL.cc
../utest/dbSta/EdenTestDBSTA.cc
../utest/antenna_checker/EdenTestANTENNA_CHECKER.cc
  OpenRoad.cc
  Main.cc
  ../utest/mpl/EdenTestMPL.cc
  ../utest/ppl/EdenTestPPL.cc
)

target_compile_options(unittest
  PRIVATE
    -Wall -Wextra -pedantic -Wcast-qual -Wredundant-decls -Wformat-security
    $<$<CXX_COMPILER_ID:Clang>:-Wno-gnu-zero-variadic-macro-arguments>
    $<$<CXX_COMPILER_ID:AppleClang>:-Wno-gnu-zero-variadic-macro-arguments>
)

# Disable compiler specific extensions like gnu++11.
set_target_properties(unittest PROPERTIES CXX_EXTENSIONS OFF)
target_compile_features(unittest PUBLIC cxx_std_17)

target_include_directories(unittest
  PUBLIC
  ../include
)

target_link_libraries(unittest
  ifp
  openroad_swig
  replace
  dpl
  fin
  rsz
  ppl
  flute
  dbSta
  OpenSTA
  opendb
  opendbtcl
  OpenRCX
  TritonCTS
  grt
  tap
  gui
  TritonRoute
  mpl
  psm
  antennachecker
  PartitionMgr
  utl
  utt
  ${TCL_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT}
  ${GTEST_LIBRARIES}
)

target_compile_definitions(unittest
  PUBLIC
  -DGTEST_ENABLE
)

install(TARGETS unittest DESTINATION bin)