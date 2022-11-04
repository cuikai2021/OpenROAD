cmake_minimum_required(VERSION 3.14)

add_library(LaurelFixer)

target_sources(LaurelFixer
  PRIVATE
    HoldFixer.cc
    SetupFixer.cc
    WireBudgeting.cc
    ClockConcurrentFixer.cc
)

target_include_directories(LaurelFixer
  PUBLIC
    include
)

target_link_libraries(LaurelFixer
  PRIVATE
    LaurelAdapter
    LaurelSpefReader
    LaurelSolver
)
