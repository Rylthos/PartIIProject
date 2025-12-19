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
struct BrickmapColour {
    uint8_t data;
    uint8_t r;
    uint8_t g;
    uint8_t b;

    bool getUsed() { return (data & 0x80) != 0; }
    void setUsed(bool v)
    {
        data &= ~0x80;
        data |= v ? 0x80 : 0x00;
    }

    uint8_t getParent() { return (data & 0x7C) != 0; }
    void setParent(uint8_t v)
    {
        data &= ~0x7C;
        data |= ((v & 0x1F) << 2);
    }

    uint8_t getType() { return data & 0x3; }
    void setType(uint8_t type)
    {
        data &= ~0x3;
        data |= (type & 0x3);
    }
};

std::tuple<std::vector<BrickgridPtr>, std::vector<Brickmap>, std::vector<BrickmapColour>>
generateBrickmap(std::stop_token stoken, std::unique_ptr<Loader>&& loader, GenerationInfo& info,
    glm::uvec3& brickgridDim, bool& finished);
}
