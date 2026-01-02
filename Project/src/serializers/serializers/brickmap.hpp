#pragma once

#include "common.hpp"

#include "generators/brickmap.hpp"
#include "generators/common.hpp"

#include <filesystem>

namespace Serializers {
std::optional<
    std::tuple<SerialInfo, std::vector<Generators::BrickgridPtr>, std::vector<Generators::Brickmap>,
        std::vector<Generators::BrickmapColour>, Modification::AnimationFrames>>
loadBrickmap(std::filesystem::path directory);

void storeBrickmap(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::BrickgridPtr> brickgrid, std::vector<Generators::Brickmap> brickmap,
    std::vector<Generators::BrickmapColour> colours, Generators::GenerationInfo generationInfo,
    const Modification::AnimationFrames& animation);
}
