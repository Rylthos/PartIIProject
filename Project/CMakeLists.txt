cmake_minimum_required(VERSION 3.27...3.31)

project(VoxelRaymarcher
  LANGUAGES CXX)

find_package(PkgConfig REQUIRED)

pkg_check_modules(Vulkan REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)

add_subdirectory(vendor)

add_subdirectory(src)
