# copy neccessary files into targetzone

file(COPY 
  regex_copy.cc 
  regex.cc 
  DESTINATION 
  ${CMAKE_BINARY_DIR}/test_file 
  FOLLOW_SYMLINK_CHAIN
)


