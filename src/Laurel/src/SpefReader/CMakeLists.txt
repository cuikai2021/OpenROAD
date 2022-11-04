cmake_minimum_required(VERSION 3.14)

# Earlier versions of flex use 'register' declarations that are deprecated
# in c++11 and illegal in c++17.
#find_package(FLEX 2.6.4)
find_package(FLEX)
find_package(BISON)

# Spef scan/parse.
flex_target(LrlSpefLex ${CMAKE_CURRENT_SOURCE_DIR}/LrlSpefLex.ll ${CMAKE_CURRENT_BINARY_DIR}/LrlSpefLex.cc
  COMPILE_FLAGS --prefix=LrlSpefLex_
  )
bison_target(LrlSpefParser ${CMAKE_CURRENT_SOURCE_DIR}/LrlSpefParse.yy ${CMAKE_CURRENT_BINARY_DIR}/LrlSpefParse.cc
  COMPILE_FLAGS --name-prefix=LrlSpefParse_
  )
add_flex_bison_dependency(LrlSpefLex LrlSpefParser)

add_library(LaurelSpefReader)

target_sources(LaurelSpefReader
  PRIVATE
    ${FLEX_LrlSpefLex_OUTPUTS}
    ${BISON_LrlSpefParser_OUTPUTS}
    SpefReaderFct.cc
)

target_include_directories(LaurelSpefReader
  PUBLIC
    include
  PRIVATE
    ${OPENSTA_HOME}
    ${OPENSTA_HOME}/include/sta
)

target_link_libraries(LaurelSpefReader
  PUBLIC
    OpenSTA
)
