#include "sparse_loader.hpp"

SparseLoader::SparseLoader(glm::uvec3 dimensions, std::unordered_map<glm::ivec3, glm::vec3> voxels)
    : Loader(dimensions), m_Voxels(voxels)
{
}

std::optional<glm::vec3> SparseLoader::getVoxel(glm::uvec3 index)
{
    if (glm::any(glm::greaterThanEqual(index, p_Dimensions))) {
        return {};
    }

    if (!m_Voxels.contains(glm::ivec3(index)))
        return {};

    return m_Voxels[glm::ivec3(index)];
}
