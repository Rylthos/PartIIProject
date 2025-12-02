#pragma once

#include "common.hpp"

#include "generators/grid.hpp"

#include <glm/glm.hpp>

#include <filesystem>

namespace Serializers {

std::pair<glm::uvec3, std::vector<Generators::GridVoxel>> loadGrid(std::filesystem::path file);

void storeGrid(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::GridVoxel> grid);

}
