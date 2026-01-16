#pragma once

#include "common.hpp"

#include "generators/common.hpp"
#include "generators/texture.hpp"

#include <filesystem>

namespace Serializers {

std::ifstream loadTextureFile(std::filesystem::path directory);

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::TextureVoxel>, Modification::AnimationFrames>>
loadTexture(std::filesystem::path directory);

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::TextureVoxel>, Modification::AnimationFrames>>
loadTexture(const std::vector<uint8_t>& data);

void storeTexture(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::TextureVoxel> voxels, Generators::GenerationInfo generationInfo,
    const Modification::AnimationFrames& animation);
}
