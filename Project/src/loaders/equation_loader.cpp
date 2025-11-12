#include "equation_loader.hpp"

EquationLoader::EquationLoader(glm::uvec3 dimensions, FunctionType function)
    : Loader(dimensions), m_Function(function)
{
}

std::optional<Voxel> EquationLoader::getVoxel(glm::uvec3 index)
{
    return m_Function(p_Dimensions, index);
}
