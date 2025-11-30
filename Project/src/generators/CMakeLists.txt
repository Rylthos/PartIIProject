add_library(generators STATIC)

add_subdirectory(generators)

target_include_directories(generators PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_options(generators PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(generators PUBLIC
  glm::glm
  loaders
)
