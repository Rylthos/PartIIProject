#include "equation_loader.hpp"
#include "glm/gtc/quaternion.hpp"

EquationLoader::EquationLoader(glm::uvec3 dimensions, FunctionType function)
    : Loader(dimensions), m_Function(function)
{
}

std::optional<Voxel> EquationLoader::getVoxel(glm::uvec3 index)
{
    if (glm::any(glm::greaterThanEqual(index, p_Dimensions))) {
        return {};
    }

    return m_Function(p_Dimensions, index);
}
