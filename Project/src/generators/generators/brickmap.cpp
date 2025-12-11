#include "brickmap.hpp"

namespace Generators {
std::pair<std::vector<BrickgridPtr>, std::vector<Brickmap>> generateBrickmap(std::stop_token stoken,
    std::unique_ptr<Loader>&& loader, GenerationInfo& info, glm::uvec3& brickgridDim,
    bool& finished)
{
    std::chrono::steady_clock timer;
    auto start = timer.now();

    glm::uvec3 dimensions = loader->getDimensions();
    brickgridDim = glm::uvec3(glm::ceil(glm::vec3(dimensions) / 8.f));

    size_t totalNodes = brickgridDim.x * brickgridDim.y * brickgridDim.z;

    std::vector<BrickgridPtr> brickgrid;
    std::vector<Brickmap> brickmaps;

    brickgrid.assign(totalNodes, 0x1);

    info.voxelCount = 0;

    size_t index = 0;
    size_t totalColours = 0;
    std::vector<uint8_t> colours;
    colours.resize(8 * 8 * 8 * 3);
    for (uint32_t bY = 0; bY < brickgridDim.y; bY++) {
        for (uint32_t bZ = 0; bZ < brickgridDim.z; bZ++) {
            for (uint32_t bX = 0; bX < brickgridDim.x; bX++) {
                if (stoken.stop_requested())
                    return { brickgrid, brickmaps };

                glm::ivec3 brickWorld = glm::ivec3(bX, bY, bZ) * 8;

                uint32_t usedColours = 0;
                uint64_t occupancy[8];

                uint64_t colourPtr = totalColours;

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

                                colours[usedColours * 3 + 0] = std::ceil(colour.r * 255);
                                colours[usedColours * 3 + 1] = std::ceil(colour.g * 255);
                                colours[usedColours * 3 + 2] = std::ceil(colour.b * 255);
                                usedColours++;
                                totalColours++;
                            }
                        }
                    }
                }

                if (usedColours > 0) {
                    Brickmap brick;
                    brick.colourPtr = colourPtr;
                    memcpy(&brick.occupancy, occupancy, sizeof(uint64_t) * 8);
                    brick.colour.resize(usedColours * 3);
                    std::copy(
                        colours.begin(), colours.begin() + (usedColours * 3), brick.colour.begin());

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

    return { brickgrid, brickmaps };
}
};
