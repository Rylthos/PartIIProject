set(SOURCE_LIST
  "main.cpp"
)

add_executable(server ${SOURCE_LIST})

target_compile_options(server
  PRIVATE -Wall -Wpedantic
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(server
    PRIVATE -g -Og
  )
endif()

# target_compile_definitions(server PRIVATE STB_IMAGE_IMPLEMENTATION)

target_link_libraries(server PRIVATE
  network

  CLI11::CLI11
)
