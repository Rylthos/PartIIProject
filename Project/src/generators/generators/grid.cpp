#include "grid.hpp"

namespace Generators {
std::vector<GridVoxel> generateGrid(std::stop_token stoken, std::unique_ptr<Loader>&& loader,
    GenerationInfo& info, glm::uvec3& dimensions, bool& finished)
{
    std::chrono::steady_clock timer;
    auto start = timer.now();

    dimensions = loader->getDimensions();

    const size_t totalNodes = dimensions.x * dimensions.y * dimensions.z;

    std::vector<GridVoxel> voxels;

    voxels.resize(dimensions.x * dimensions.y * dimensions.z);
    for (size_t y = 0; y < dimensions.y; y++) {
        for (size_t z = 0; z < dimensions.z; z++) {
            for (size_t x = 0; x < dimensions.x; x++) {
                if (stoken.stop_requested())
                    return voxels;

                size_t index = x + z * dimensions.x + y * dimensions.x * dimensions.z;

                {
                    info.completionPercent = (index + 1) / (float)totalNodes;

                    auto current = timer.now();
                    std::chrono::duration<float, std::milli> difference = current - start;
                    info.generationTime = difference.count() / 1000.0f;
                }

                auto v = loader->getVoxel({ x, y, z });

                if (v.has_value()) {
                    glm::vec3 colour = v.value();
                    voxels[index] = GridVoxel {
                        .visible = true,
                        .colour = glm::u8vec3 {
                            (uint8_t)(colour.r * 255.f),
                            (uint8_t)(colour.g * 255.f),
                            (uint8_t)(colour.b * 255.f),
                        }
                    };
                } else {
                    voxels[index] = GridVoxel {
                        .visible = false,
                        .colour = glm::u8vec3(0),
                    };
                }
            }
        }
    }

    auto end = timer.now();
    std::chrono::duration<float, std::milli> difference = end - start;
    info.generationTime = difference.count() / 1000.0f;

    finished = true;

    info.voxelCount = voxels.size();
    info.nodes = voxels.size();

    return voxels;
}
}
