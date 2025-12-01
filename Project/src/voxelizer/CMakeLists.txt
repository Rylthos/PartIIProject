set(SOURCE_LIST
  "main.cpp"
  "parser.hpp" "parser.cpp"
)

add_executable(Voxelizer ${SOURCE_LIST})

target_compile_options(Voxelizer
  PRIVATE -Wall -Wpedantic
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(Voxelizer
    PRIVATE -g -Og
  )
endif()

if (CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
  set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

target_link_libraries(Voxelizer PRIVATE
  CLI11::CLI11
  glm::glm
)
