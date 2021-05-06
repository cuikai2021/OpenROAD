# cmake list for build unit test tool

add_library(utt)

target_sources(utt
  PRIVATE
    src/itest.cpp
)
  
target_include_directories(utt
  PUBLIC
    include
)

target_link_libraries(utt
  PUBLIC
  ${GTEST_LIBRARIES}
)

