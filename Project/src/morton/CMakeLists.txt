add_library(morton STATIC)

add_subdirectory(morton)

target_include_directories(morton PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_options(morton PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(morton PRIVATE
  glm::glm
)
