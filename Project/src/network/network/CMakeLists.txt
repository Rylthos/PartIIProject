target_sources(network PRIVATE
  "setup.hpp" "setup.cpp"
  "header.hpp" "header.cpp"
  "serializers.hpp" "serializers.cpp"
)

add_subdirectory(handlers)
