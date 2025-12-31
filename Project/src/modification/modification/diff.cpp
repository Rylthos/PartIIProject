#include "diff.hpp"

namespace Modification {

DiffType getDiff(std::optional<glm::vec3> first, std::optional<glm::vec3> second)
{
    if (first.has_value() && second.has_value()) {
        if (first.value() != second.value()) {
            return std::make_pair(Type::REPLACE, ShapeInfo { Shape::VOXEL, second.value() });
        } else {
            return {};
        }
    } else if (!first.has_value() && second.has_value()) {
        return std::make_pair(Type::PLACE, ShapeInfo { Shape::VOXEL, second.value() });
    } else if (first.has_value() && !second.has_value()) {
        return std::make_pair(Type::ERASE, ShapeInfo { Shape::VOXEL });
    } else if (!first.has_value() && !second.has_value()) {
        return {};
    }

    return {};
}

}
