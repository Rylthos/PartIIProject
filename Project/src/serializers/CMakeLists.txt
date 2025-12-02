add_library(serializers STATIC)

add_subdirectory(serializers)

target_include_directories(serializers PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_options(serializers PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(serializers PUBLIC
  glm::glm
  generators
  logger
)
