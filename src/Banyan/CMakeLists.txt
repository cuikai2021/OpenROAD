include("openroad")

project(Banyan
  LANGUAGES CXX
)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

file(GLOB_RECURSE PROTO_FILES banyan-integrate/*.proto)
protoc(banyan_pb
  ${PROTO_FILES}
  PB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/banyan-integrate/"
  CPP_OUT "${CMAKE_CURRENT_BINARY_DIR}/cpp_out/"
)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)

find_package(Boost 1.68.0)
find_package(Boost 1.68.0 COMPONENTS program_options REQUIRED)
find_package(OpenMP REQUIRED)

swig_lib(NAME Banyan
  NAMESPACE banyan
  I_FILE src/Banyan.i
  SCRIPTS src/Banyan.tcl
)

target_sources(Banyan
  PRIVATE
  src/Banyan.cpp
  src/MakeBanyan.cpp
  src/DBAdapter.cpp
  src/Global.cpp
  src/MergeWireSegment.cpp
  src/GridlessMaze.cpp
  src/PinCluster.cpp
)

target_include_directories(Banyan
  PUBLIC
  include

  PRIVATE
    src
)

target_link_libraries(Banyan
  PUBLIC
    dl
    odb
    utl
    rsz
    stt
    Laurel
    OpenSTA
    dbSta
    Threads::Threads
    OpenMP::OpenMP_CXX
    Boost::boost
    ${Boost_PROGRAM_OPTIONS_LIBRARIES}
    ZLIB::ZLIB
    fassert
    prettyprint

  PRIVATE
    banyan_pb
)

add_subdirectory(test)

# ###########################################################
# Compiler flags
# ###########################################################
# Todo: add -Wextra and cleanup warnings
target_compile_options(Banyan
  PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:-Wall -pedantic -Wcast-qual -Wredundant-decls -Wformat-security>
  $<$<CXX_COMPILER_ID:AppleClang>:-Wall -pedantic -Wcast-qual -Wredundant-decls -Wformat-security -Wno-gnu-zero-variadic-macro-arguments>
  $<$<CXX_COMPILER_ID:Clang>:-Wall -pedantic -Wcast-qual -Wredundant-decls -Wformat-security -Wno-gnu-zero-variadic-macro-arguments>
)

target_compile_features(Banyan PRIVATE cxx_std_17)
