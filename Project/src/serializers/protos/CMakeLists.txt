add_library(serializer-proto OBJECT
  "proto/as_proto/general.proto"
  "proto/as_proto/grid.proto"
)

target_link_libraries(serializer-proto PUBLIC protobuf::libprotobuf)

set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

target_include_directories(serializer-proto PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")

protobuf_generate(
  LANGUAGE cpp
  TARGET serializer-proto
  IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}/proto"
  PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}"
)
