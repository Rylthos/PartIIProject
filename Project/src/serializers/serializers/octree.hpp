#pragma once

#include "common.hpp"

#include "generators/common.hpp"
#include "generators/octree.hpp"

#include <filesystem>

namespace Serializers {
std::optional<std::tuple<SerialInfo, std::vector<Generators::OctreeNode>>> loadOctree(
    std::filesystem::path directory);

void storeOctree(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::OctreeNode> nodes, Generators::GenerationInfo generationInfo);
}
