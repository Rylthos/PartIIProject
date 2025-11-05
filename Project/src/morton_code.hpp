#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace MortonCode {
uint64_t encode(glm::uvec3 position);
glm::uvec3 decode(uint64_t code);
}
