#pragma once

#include "common.hpp"

#include "generators/common.hpp"
#include "generators/contree.hpp"

#include <filesystem>

namespace Serializers {

std::ifstream loadContreeFile(std::filesystem::path directory);

std::optional<std::tuple<SerialInfo, std::vector<Generators::ContreeNode>>> loadContree(
    std::filesystem::path directory);

std::optional<std::tuple<SerialInfo, std::vector<Generators::ContreeNode>>> loadContree(
    const std::vector<uint8_t>& data);

void storeContree(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::ContreeNode> nodes, Generators::GenerationInfo generationInfo);
}
