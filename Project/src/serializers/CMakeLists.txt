add_library(serializers STATIC)

add_subdirectory(serializers)
add_subdirectory(protos)

target_include_directories(serializers PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_options(serializers PRIVATE -Wall -Wextra -Wpedantic)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  message("Debug")
  target_compile_options(serializers
    PRIVATE -g -Og
  )
endif()

target_link_libraries(serializers PUBLIC
  generators
  logger
  modification

  serializer-proto

  glm::glm
)
