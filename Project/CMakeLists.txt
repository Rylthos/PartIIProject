cmake_minimum_required(VERSION 3.27...3.31)

project(VoxelRaymarcher
  LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_subdirectory(vendor)

add_subdirectory(src)
