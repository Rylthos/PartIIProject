add_library(Renderer-proto OBJECT
  "proto/network_proto/header.proto"
  "proto/network_proto/frame.proto"
  "proto/network_proto/update.proto"
  "proto/network_proto/stream_header.proto"
  "proto/network_proto/datagram_header.proto"
  "proto/network_proto/messages.proto"
)

target_link_libraries(Renderer-proto PUBLIC protobuf::libprotobuf)

set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

target_include_directories(Renderer-proto PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")

protobuf_generate(
  LANGUAGE cpp
  TARGET Renderer-proto
  IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}/proto"
  PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}"
)
