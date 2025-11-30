add_library(loaders STATIC)

add_subdirectory(loaders)

target_include_directories(loaders PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(loaders PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(loaders PUBLIC
  logger
  morton
  glm::glm
)
