cmake_minimum_required(VERSION 3.14)

include("openroad")

add_library(LaurelAdapter)

target_sources(LaurelAdapter
  PRIVATE
    LaurelAdapter.cc
    Buffering.cc
    Replacer.cc
    DrvrBfsBkwdIterator.cc
)

set(LARUEL_ADAPTER_PATH ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(LaurelAdapter
  PUBLIC
    include
    ${OPENROAD_HOME}/include
    ${OPENSTA_HOME}
    ${OPENSTA_HOME}/include/sta
  PRIVATE
    .
)

target_link_libraries(LaurelAdapter
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
)
