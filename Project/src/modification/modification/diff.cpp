#include "diff.hpp"
#include "glm/fwd.hpp"

namespace Modification {

std::optional<DiffType> getDiff(std::optional<glm::vec3> first, std::optional<glm::vec3> second)
{
    if (first.has_value() && second.has_value()) {
        if (first.value() != second.value()) {
            return std::make_pair(Type::REPLACE, second.value());
        } else {
            return {};
        }
    } else if (!first.has_value() && second.has_value()) {
        return std::make_pair(Type::PLACE, second.value());
    } else if (first.has_value() && !second.has_value()) {
        return std::make_pair(Type::ERASE, glm::vec3(0.f));
    } else if (!first.has_value() && !second.has_value()) {
        return {};
    }

    return {};
}

}
