if (CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
  set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

add_subdirectory(morton)
add_subdirectory(events)
add_subdirectory(logger)
add_subdirectory(loaders)
add_subdirectory(generators)
add_subdirectory(renderer)

add_subdirectory(voxelizer)
