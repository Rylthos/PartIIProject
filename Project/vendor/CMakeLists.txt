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

# Slang
set(SLANG_ENABLE_TESTS              OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_EXAMPLES           OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_REPLAYER           OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_PREBUILT_BINARIES  OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_GFX                OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANGD             OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANGC             OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANGI             OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANGRT            OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_XLIB               OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANG_RHI          OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_DXIL               OFF CACHE BOOL "" FORCE)
set(SLANG_EMBED_CORE_MODULE         OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_FULL_IR_VALIDATION OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_IR_BREAK_ALLOC     OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_ASAN               OFF CACHE BOOL "" FORCE)

add_subdirectory(slang)
