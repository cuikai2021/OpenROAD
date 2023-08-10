cmake_minimum_required(VERSION 3.14)

include(CompileProtobuf)

file(GLOB_RECURSE PROTO_FILES *.proto)
protoc(rsz_pb
  ${PROTO_FILES}
  PB_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
  CPP_OUT "${CMAKE_CURRENT_BINARY_DIR}/cpp_out/"
)
