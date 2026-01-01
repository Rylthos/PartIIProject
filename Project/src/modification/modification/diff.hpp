#pragma once

#include "mod_type.hpp"

#include <optional>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

namespace Modification {

typedef std::pair<Type, glm::vec3> DiffType;
typedef std::vector<std::unordered_map<glm::ivec3, DiffType>> AnimationFrames;

std::optional<DiffType> getDiff(std::optional<glm::vec3> first, std::optional<glm::vec3> second);
}
