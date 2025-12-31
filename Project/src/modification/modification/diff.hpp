#pragma once

#include "mod_type.hpp"

#include <optional>

#include <glm/glm.hpp>

namespace Modification {

typedef std::optional<std::pair<Type, ShapeInfo>> DiffType;

DiffType getDiff(std::optional<glm::vec3> first, std::optional<glm::vec3> second);
}
