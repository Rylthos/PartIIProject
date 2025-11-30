set(SOURCE_LIST
  "main.cpp"
  "parser.hpp" "parser.cpp"
)

add_executable(Voxelizer ${SOURCE_LIST})

target_compile_options(Voxelizer
  PRIVATE -Wall -Wpedantic
)

target_link_libraries(Voxelizer PRIVATE
  CLI11::CLI11
)
