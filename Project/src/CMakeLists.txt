set (SOURCE_LIST
  "main.cpp"
  "application.cpp" "application.hpp"
  "window.cpp" "window.hpp"
  "logger.cpp" "logger.hpp"
)

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
)
