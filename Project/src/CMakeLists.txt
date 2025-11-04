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
)

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

add_executable(${PROJECT_NAME} ${SOURCE_LIST})

add_subdirectory(accelerationStructures)
add_subdirectory(loaders)

target_compile_options(${PROJECT_NAME}
  PRIVATE -g -O
)

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
)
