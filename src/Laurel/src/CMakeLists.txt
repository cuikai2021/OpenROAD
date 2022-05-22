cmake_minimum_required(VERSION 3.14)

find_package(ortools CONFIG REQUIRED)

include("openroad")

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
)

target_include_directories(Laurel
  PUBLIC
    ../include
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
