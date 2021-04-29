# include("openroad")

# link_libraries(checker)

# include_directories(${CHECKER_INCLUDE})

# add_executable(ut_ppl TestMpl.cc)

include("openroad")
include_directories(../src)

# swig_lib(NAME      ut_ppl
#          NAMESPACE ut_ppl
#          I_FILE    ../src/MacroPlacer.i
# )

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/unittest)
add_executable(ut_ppl TestPpl.cc)

target_sources(ut_ppl
  PRIVATE
    TestPpl.cc
)

# target_link_libraries(ut_ppl
#   PUBLIC
#   utt
#   mpl
#   ${GTEST_LIBRARIES}
# )

target_link_libraries(ut_ppl
  utt
  ppl
)