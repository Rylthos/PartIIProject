#pragma once

#include <glm/glm.hpp>

#include <memory>
#include <thread>
#include <vector>

#include "generators/common.hpp"
#include "loaders/loader.hpp"

namespace Generators {
using TextureVoxel = glm::u8vec4;

std::vector<TextureVoxel> generateTexture(std::stop_token stoken, std::unique_ptr<Loader>&& loader,
    GenerationInfo& info, glm::uvec3& dimensions, bool& finished);
}
