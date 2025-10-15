set (SOURCE_LIST
  "main.cpp"
  "application.cpp" "application.hpp"
  "window.cpp" "window.hpp"
  "logger.cpp" "logger.hpp"
  "event_watcher.cpp" "event_watcher.hpp"
  "events.hpp"
  "shader_manager.cpp" "shader_manager.hpp"
  "file_watcher.cpp" "file_watcher.hpp"
)

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

add_executable(${PROJECT_NAME} ${SOURCE_LIST})

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
