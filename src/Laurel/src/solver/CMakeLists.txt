cmake_minimum_required(VERSION 3.14)

find_package(ortools CONFIG REQUIRED)

add_library(LaurelSolver)

target_sources(LaurelSolver
  PRIVATE
    LPSolver.cc
    MCFSolver.cc
)

target_include_directories(LaurelSolver
  PUBLIC
    include
)

target_link_libraries(LaurelSolver
  PRIVATE
    ortools::ortools
)
