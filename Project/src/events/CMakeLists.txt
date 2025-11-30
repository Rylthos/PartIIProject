add_library(events STATIC)

add_subdirectory(events)

target_include_directories(events PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_options(events PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(events PUBLIC
  glm::glm
)
