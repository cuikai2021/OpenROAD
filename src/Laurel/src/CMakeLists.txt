cmake_minimum_required(VERSION 3.14)

find_package(ortools CONFIG REQUIRED)

# Earlier versions of flex use 'register' declarations that are deprecated
# in c++11 and illegal in c++17.
#find_package(FLEX 2.6.4)
find_package(FLEX)
find_package(BISON)

include("openroad")

set(STA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../sta)

# Spef scan/parse.
flex_target(LrlSpefLex ${CMAKE_CURRENT_SOURCE_DIR}/SpefReader/LrlSpefLex.ll ${CMAKE_CURRENT_BINARY_DIR}/LrlSpefLex.cc
  COMPILE_FLAGS --prefix=LrlSpefLex_
  )
bison_target(LrlSpefParser ${CMAKE_CURRENT_SOURCE_DIR}/SpefReader/LrlSpefParse.yy ${CMAKE_CURRENT_BINARY_DIR}/LrlSpefParse.cc
  COMPILE_FLAGS --name-prefix=LrlSpefParse_
  )
add_flex_bison_dependency(LrlSpefLex LrlSpefParser)

swig_lib(NAME      Laurel
         NAMESPACE lrl
         I_FILE    Laurel.i
         SCRIPTS   Laurel.tcl
)

target_sources(Laurel
  PRIVATE
    Laurel.cc
    HoldFixer.cc
    SetupFixer.cc
    MakeLaurel.cc
    WireBudgeting.cc
    FastOpt.cc
    ClockConcurrentFixer.cc

    ${FLEX_LrlSpefLex_OUTPUTS}
    ${BISON_LrlSpefParser_OUTPUTS}
    SpefReader/SpefReaderFct.cc
)

target_include_directories(Laurel
  PUBLIC
    ../include
  PRIVATE
    ${STA_DIR}
    ${STA_DIR}/include/sta
)

target_link_libraries(Laurel
  PUBLIC
    stt
    odb
    OpenSTA
    dbSta
    grt
    gui
    utl
    rsz
    spdlog::spdlog
    ortools::ortools
)

messages(
  TARGET Laurel
  OUTPUT_DIR ..
)
