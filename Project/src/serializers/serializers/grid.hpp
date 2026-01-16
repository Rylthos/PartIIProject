#pragma once

#include "common.hpp"

#include "generators/grid.hpp"

#include "modification/diff.hpp"

#include <glm/glm.hpp>

#include <filesystem>

namespace Serializers {

std::ifstream loadGridFile(std::filesystem::path directory);

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::GridVoxel>, Modification::AnimationFrames>>
loadGrid(std::filesystem::path directory);

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::GridVoxel>, Modification::AnimationFrames>>
loadGrid(const std::vector<uint8_t>& data);

void storeGrid(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::GridVoxel> grid, Generators::GenerationInfo generationInfo,
    const Modification::AnimationFrames& animation);

}
