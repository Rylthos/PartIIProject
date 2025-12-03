#pragma once

#include "common.hpp"

#include "generators/common.hpp"
#include "generators/texture.hpp"

#include <filesystem>

namespace Serializers {
std::optional<std::tuple<SerialInfo, std::vector<Generators::TextureVoxel>>> loadTexture(
    std::filesystem::path directory);

void storeTexture(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::TextureVoxel> voxels, Generators::GenerationInfo generationInfo);
}
