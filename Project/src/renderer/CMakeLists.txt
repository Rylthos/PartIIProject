set (SOURCE_LIST
  "main.cpp"
  "application.cpp" "application.hpp"
  "window.cpp" "window.hpp"
  "shader_manager.cpp" "shader_manager.hpp"
  "file_watcher.cpp" "file_watcher.hpp"
  "camera.cpp" "camera.hpp"
  "ring_buffer.hpp"
  "debug_utils.cpp" "debug_utils.hpp"
  "acceleration_structure_manager.cpp" "acceleration_structure_manager.cpp"
  "buffer.cpp" "buffer.hpp"
  "tracing.hpp"
  "frame_commands.cpp" "frame_commands.hpp"
  "pipeline_layout.cpp" "pipeline_layout.hpp"
  "compute_pipeline.cpp" "compute_pipeline.hpp"
  "descriptor_layout.cpp" "descriptor_layout.hpp"
  "descriptor_set.cpp" "descriptor_set.hpp"
  "image.cpp" "image.hpp"
  "scene_manager.cpp" "scene_manager.hpp"
  "performance_logger.cpp" "performance_logger.hpp"
  "modification_manager.cpp" "modification_manager.hpp"
  "animation_manager.cpp" "animation_manager.hpp"
)

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

add_executable(${PROJECT_NAME} ${SOURCE_LIST})

target_compile_definitions(${PROJECT_NAME} PRIVATE SERVER_CLIENT)

add_subdirectory(accelerationStructures)

target_compile_options(${PROJECT_NAME}
  PUBLIC
    -Wnullability-extension
  PRIVATE -Wall -Wpedantic
)


if (RUN_SERVER STREQUAL "1")
  message("Building for server")
  add_compile_definitions(SERVER_CLIENT)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(${PROJECT_NAME}
    PRIVATE -g -Og
  )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
  loaders
  logger
  modification
  generators
  events
  serializers
  network

  glm::glm
  Vulkan::UtilityHeaders
  spdlog
  vk-bootstrap
  vulkan
  glfw
  slang
  ImGui
  Tracy::TracyClient
  GPUOpen::VulkanMemoryAllocator
  nlohmann_json::nlohmann_json
  CLI11::CLI11
)
