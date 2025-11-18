set (SOURCE_LIST
  "main.cpp"
  "application.cpp" "application.hpp"
  "window.cpp" "window.hpp"
  "logger.cpp" "logger.hpp"
  "event_dispatcher.cpp" "event_dispatcher.hpp"
  "events.hpp"
  "shader_manager.cpp" "shader_manager.hpp"
  "file_watcher.cpp" "file_watcher.hpp"
  "camera.cpp" "camera.hpp"
  "ring_buffer.hpp"
  "debug_utils.cpp" "debug_utils.hpp"
  "acceleration_structure_manager.cpp" "acceleration_structure_manager.cpp"
  "buffer.cpp" "buffer.hpp"
  "voxel.hpp"
  "morton_code.cpp" "morton_code.hpp"
  "tracing.hpp"
  "frame_commands.cpp" "frame_commands.hpp"
  "pipeline_layout.cpp" "pipeline_layout.hpp"
)

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

add_executable(${PROJECT_NAME} ${SOURCE_LIST})

add_subdirectory(accelerationStructures)
add_subdirectory(loaders)

target_compile_options(${PROJECT_NAME}
  PRIVATE -Wall -Wpedantic
)

if (CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
  set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(${PROJECT_NAME}
    PRIVATE -g -Og
  )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
  glm::glm
  GPUOpen::VulkanMemoryAllocator
  Vulkan::UtilityHeaders
  spdlog
  vk-bootstrap
  vulkan
  glfw
  slang
  ImGui
  Tracy::TracyClient
)
