cmake_minimum_required(VERSION 3.27...3.31)

project(VoxelRaymarcher
  LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

set (CMAKE_CXX_STANDARDA 20)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  message("Enabled Debug features")
  add_compile_definitions(DEBUG)
endif()

add_subdirectory(vendor)

add_subdirectory(src)
