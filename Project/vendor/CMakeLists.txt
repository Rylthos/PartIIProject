# GLM
set(GLM_QUIET
    OFF
    CACHE BOOL "" FORCE)
set(GLM_TEST_ENABLE
    OFF
    CACHE BOOL "" FORCE)
add_subdirectory(glm)

# vk-bootstrap
set(VK_BOOTSTRAP_DISABLE_WARNINGS
    ON
    CACHE BOOL "" FORCE)
add_subdirectory(vk-bootstrap)

# VMA
add_subdirectory(VMA)

# spdlog
add_subdirectory(spdlog)

# Vulkan Utility Libraries
add_subdirectory(VulkanUtilityLibraries)
