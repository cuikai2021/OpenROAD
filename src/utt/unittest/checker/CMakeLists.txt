# cmake_minimum_required(VERSION 3.10)

# link_libraries(checker)

file(COPY regex_copy.cc regex.cc DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/unittest FOLLOW_SYMLINK_CHAIN)

include_directories(${CHECKER_INCLUDE})

# swig_lib(NAME      ut_utt
#          NAMESPACE ut_utt
#          I_FILE    TestChecker.i
# )

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/unittest)
add_executable(ut_utt TestChecker.cc)

target_sources(ut_utt
  PRIVATE
    TestChecker.cc
)

target_link_libraries(ut_utt
  PUBLIC
  utt
  ${GTEST_LIBRARIES}
)

# add_executable(regex regex.cc)


