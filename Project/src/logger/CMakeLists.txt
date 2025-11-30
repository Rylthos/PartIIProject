add_library(logger STATIC)

add_subdirectory(logger)

target_include_directories(logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(logger PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(logger PUBLIC
  events

  spdlog
  ImGui
  Vulkan::UtilityHeaders
)
