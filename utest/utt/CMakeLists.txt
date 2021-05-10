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

file(COPY 
  test_file/regex_copy.txt
  test_file/regex.txt
  DESTINATION 
  ${CMAKE_BINARY_DIR}/src/test_file 
)