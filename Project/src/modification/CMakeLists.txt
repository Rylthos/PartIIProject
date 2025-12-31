add_library(modification STATIC)

add_subdirectory(modification)

target_include_directories(modification PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_options(modification PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(modification PRIVATE
  glm::glm
)
