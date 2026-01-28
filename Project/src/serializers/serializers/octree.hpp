#pragma once

#include "common.hpp"

#include "generators/common.hpp"
#include "generators/octree.hpp"

#include <filesystem>
#include <fstream>

namespace Serializers {

std::ifstream loadOctreeFile(std::filesystem::path directory);

std::optional<std::tuple<SerialInfo, std::vector<Generators::OctreeNode>>> loadOctree(
    std::filesystem::path directory);

std::optional<std::tuple<SerialInfo, std::vector<Generators::OctreeNode>>> loadOctree(
    const std::vector<uint8_t>& data);

void storeOctree(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::OctreeNode> nodes, Generators::GenerationInfo generationInfo);
}
