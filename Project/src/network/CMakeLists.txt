add_library(network STATIC)

add_subdirectory(network)

target_include_directories(network PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_options(network PRIVATE -Wall -Wextra -Wpedantic)
