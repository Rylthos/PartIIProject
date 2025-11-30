#pragma once

#include <glm/glm.hpp>

#include <memory>
#include <thread>
#include <vector>

#include "common.hpp"
#include "loaders/loader.hpp"

namespace Generators {
struct GridVoxel {
    bool visible;
    glm::vec3 colour;
};

std::vector<GridVoxel> generateGrid(std::stop_token stoken, std::unique_ptr<Loader>&& loader,
    GenerationInfo& info, glm::uvec3& dimensions, bool& finished);
}
