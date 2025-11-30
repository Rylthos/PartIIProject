#pragma once

#include <glm/glm.hpp>

#include <memory>
#include <thread>
#include <vector>

#include "common.hpp"
#include "loaders/loader.hpp"

namespace Generators {
using BrickgridPtr = uint32_t; // Highest bit marks loaded

struct Brickmap {
    uint64_t colourPtr;
    uint64_t occupancy[8];
    std::vector<uint8_t> colour;
};

std::pair<std::vector<BrickgridPtr>, std::vector<Brickmap>> generateBrickmap(std::stop_token stoken,
    std::unique_ptr<Loader>&& loader, GenerationInfo& info, glm::uvec3& brickgridDim,
    bool& finished);
}
