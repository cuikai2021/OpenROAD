cmake_minimum_required(VERSION 3.14)

include("openroad")

swig_lib(NAME      Laurel
NAMESPACE lrl
I_FILE    Laurel.i
SCRIPTS   Laurel.tcl
)

set(LARUEL_SRC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(adapter)
add_subdirectory(SpefReader)
add_subdirectory(solver)
add_subdirectory(fixer)

target_sources(Laurel
  PRIVATE
    Laurel.cc
    FastOpt.cc
    MakeLaurel.cc
)

target_include_directories(Laurel
  PUBLIC
    ${LARUEL_HOME}/include
)

target_link_libraries(Laurel
  PUBLIC
    LaurelAdapter
  PRIVATE
    LaurelSpefReader
    LaurelSolver
    LaurelFixer
)

messages(
  TARGET Laurel
  OUTPUT_DIR ..
)
