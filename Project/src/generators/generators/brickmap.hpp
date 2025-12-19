#pragma once

#include <glm/glm.hpp>

#include <memory>
#include <thread>
#include <vector>

#include "common.hpp"
#include "loaders/loader.hpp"

namespace Generators {
// Lowest bit marks loaded
// Second lowest marks requested
using BrickgridPtr = uint32_t;

struct Brickmap {
    uint64_t colourPtr;
    uint64_t occupancy[8];
};

// Types:
//  0 -> 8x8x8
//  1 -> 4x4x4
//  2 -> 2x2x2
union BrickmapColour {
    uint8_t data[4];
    struct {
        uint8_t used   : 1;
        uint8_t parent : 5;
        uint8_t type   : 2;
        uint8_t r;
        uint8_t g;
        uint8_t b;
    } components;
};

std::tuple<std::vector<BrickgridPtr>, std::vector<Brickmap>, std::vector<BrickmapColour>>
generateBrickmap(std::stop_token stoken, std::unique_ptr<Loader>&& loader, GenerationInfo& info,
    glm::uvec3& brickgridDim, bool& finished);
}
