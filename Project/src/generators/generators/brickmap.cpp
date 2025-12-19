#include "brickmap.hpp"
#include <optional>

namespace Generators {
uint32_t getOffset(uint32_t type)
{
    switch (type) {
    case 2:
        return 8;
    case 1:
        return 64;
    case 0:
        return 512;
    default:
        assert(false && "Type out of range");
    }
}

uint32_t getFreeColour(std::array<uint8_t, 8 * 8 * 8 * 3>& brickColours, uint32_t usedColours,
    std::vector<BrickmapColour>& colours, uint32_t start_index = 0)
{
    uint32_t type;
    if (usedColours <= 2 * 2 * 2) {
        type = 2;
    } else if (usedColours <= 4 * 4 * 4) {
        type = 1;
    } else {
        type = 0;
    }

    // Traverse colours
    for (uint32_t i = start_index; i < colours.size();) {
        if (colours[i].getUsed() || colours[i].getType() > type) {
            i += getOffset(type);

            continue;
        }

        if (colours[i].getType() == type) { // Can use
            colours[i].setUsed(true);

            for (uint32_t j = 0; j < usedColours; j++) {
                colours[i + j].r = brickColours[j * 3 + 0];
                colours[i + j].g = brickColours[j * 3 + 1];
                colours[i + j].b = brickColours[j * 3 + 2];
            }

            return i;
        } else if (colours[i].getType() < type) { // Split node
            uint32_t newType = colours[i].getType() + 1;
            uint32_t offset = getOffset(newType);
            colours[i].setType(newType);
            for (int j = 0; j < 8; j++) {
                colours[i + j * offset].setType(newType);
            }
        } else {
            assert(false && "Should not be reachable");
        }
    }

    // Didn't find anything free so resize
    uint32_t end = colours.size();
    colours.resize(end + 512);
    colours[end].setType(0);
    colours[end].setUsed(false);
    return getFreeColour(brickColours, usedColours, colours, end);
}

std::tuple<std::vector<BrickgridPtr>, std::vector<Brickmap>, std::vector<BrickmapColour>>
generateBrickmap(std::stop_token stoken, std::unique_ptr<Loader>&& loader, GenerationInfo& info,
    glm::uvec3& brickgridDim, bool& finished)
{
    std::chrono::steady_clock timer;
    auto start = timer.now();

    glm::uvec3 dimensions = loader->getDimensions();
    brickgridDim = glm::uvec3(glm::ceil(glm::vec3(dimensions) / 8.f));

    size_t totalNodes = brickgridDim.x * brickgridDim.y * brickgridDim.z;

    std::vector<BrickgridPtr> brickgrid;
    std::vector<Brickmap> brickmaps;
    std::vector<BrickmapColour> colours;

    colours.resize(512);
    colours[0].setUsed(false);
    colours[0].setType(0);

    brickgrid.assign(totalNodes, 0x1);

    info.voxelCount = 0;

    size_t index = 0;
    std::array<uint8_t, 8 * 8 * 8 * 3> brickColours;
    for (uint32_t bY = 0; bY < brickgridDim.y; bY++) {
        for (uint32_t bZ = 0; bZ < brickgridDim.z; bZ++) {
            for (uint32_t bX = 0; bX < brickgridDim.x; bX++) {
                if (stoken.stop_requested())
                    return { brickgrid, brickmaps, colours };

                glm::ivec3 brickWorld = glm::ivec3(bX, bY, bZ) * 8;

                uint32_t usedColours = 0;
                uint64_t occupancy[8];

                {
                    info.completionPercent = (index + 1) / (float)totalNodes;
                    auto current = timer.now();
                    std::chrono::duration<float, std::milli> difference = current - start;
                    info.generationTime = difference.count() / 1000.0f;
                }

                for (uint64_t y = 0; y < 8; y++) {
                    occupancy[y] = 0;
                    for (uint64_t z = 0; z < 8; z++) {
                        for (uint64_t x = 0; x < 8; x++) {
                            glm::ivec3 coordinates = brickWorld + glm::ivec3(x, y, z);

                            auto voxel = loader->getVoxel(coordinates);

                            if (voxel.has_value()) {
                                occupancy[y] |= ((uint64_t)1) << ((z * 8) + x);

                                glm::vec3 colour = voxel.value();

                                brickColours[usedColours * 3 + 0] = std::ceil(colour.r * 255);
                                brickColours[usedColours * 3 + 1] = std::ceil(colour.g * 255);
                                brickColours[usedColours * 3 + 2] = std::ceil(colour.b * 255);
                                usedColours++;
                            }
                        }
                    }
                }

                if (usedColours > 0) {
                    Brickmap brick;
                    brick.colourPtr = getFreeColour(brickColours, usedColours, colours);
                    memcpy(&brick.occupancy, occupancy, sizeof(uint64_t) * 8);
                    brickmaps.push_back(brick);
                    brickgrid[index] = 0x1 | (brickmaps.size() << 2);
                    info.voxelCount += 8 * 8 * 8;
                }

                index++;
            }
        }
    }

    auto end = timer.now();
    std::chrono::duration<float, std::milli> difference = end - start;
    info.generationTime = difference.count() / 1000.0f;

    info.nodes = brickgrid.size() + brickmaps.size();

    finished = true;

    return { brickgrid, brickmaps, colours };
}
};
