add_library(stb_lib STATIC)
set_target_properties(stb_lib PROPERTIES LINKER_LANGUAGE C)


add_subdirectory(stb)

target_include_directories(stb_lib PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_options(stb_lib PRIVATE -Wall -Wextra -Wpedantic)
