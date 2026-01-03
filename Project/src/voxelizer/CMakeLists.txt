set(SOURCE_LIST
  "main.cpp"
  "parser.hpp" "parser.cpp"
  "parser_args.hpp"
)

add_executable(Voxelizer ${SOURCE_LIST})

add_subdirectory(parsers)

target_compile_options(Voxelizer
  PRIVATE -Wall -Wpedantic
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(Voxelizer
    PRIVATE -g -Og
  )
endif()

# target_compile_definitions(Voxelizer PRIVATE STB_IMAGE_IMPLEMENTATION)

target_link_libraries(Voxelizer PRIVATE
  loaders
  modification
  generators
  serializers

  CLI11::CLI11
  glm::glm
  stb_lib
  pgbar
)
