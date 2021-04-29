set(CHECKER_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include" CACHE INTERNAL "GLOBAL_INCLUDES" )
# add_library(checker SHARED Checker.cpp)

# target_link_libraries(checker ${GTEST_LIBRARIES})


add_library(utt)

add_subdirectory(unittest/checker)

include_directories(..)

target_sources(utt
  PRIVATE
    src/Checker.cpp
)
  
target_include_directories(utt
  PUBLIC
    include
)

target_link_libraries(utt
  PUBLIC
  ${GTEST_LIBRARIES}
)

