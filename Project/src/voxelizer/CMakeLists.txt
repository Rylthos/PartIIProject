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

target_link_libraries(Voxelizer PRIVATE
  loaders
  generators
  serializers

  CLI11::CLI11
  glm::glm
)
